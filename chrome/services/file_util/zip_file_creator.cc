// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/zip_file_creator.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/services/filesystem/public/mojom/types.mojom-shared.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/zlib/google/zip.h"

namespace chrome {

namespace {

// A zip::FileAccessor that talks to a file system through the Mojo
// filesystem::mojom::Directory.
// Note that zip::ZipFileAccessor deals with absolute paths that must be
// converted to relative when calling filesystem::mojom::Directory APIs.
class MojoFileAccessor : public zip::FileAccessor {
 public:
  MojoFileAccessor(
      mojo::PendingRemote<filesystem::mojom::Directory> source_dir_remote)
      : source_dir_remote_(std::move(source_dir_remote)) {}

  ~MojoFileAccessor() override = default;

  bool Open(const zip::Paths paths,
            std::vector<base::File>* const files) override {
    std::vector<filesystem::mojom::FileOpenDetailsPtr> details;
    details.reserve(paths.size());

    for (const base::FilePath& path : paths) {
      DCHECK(!path.IsAbsolute());
      filesystem::mojom::FileOpenDetailsPtr open_details =
          filesystem::mojom::FileOpenDetails::New();
      open_details->path = path.value();
      open_details->open_flags =
          filesystem::mojom::kFlagOpen | filesystem::mojom::kFlagRead;
      details.push_back(std::move(open_details));
    }

    std::vector<filesystem::mojom::FileOpenResultPtr> results;
    if (!source_dir_remote_->OpenFileHandles(std::move(details), &results))
      return false;

    files->reserve(files->size() + results.size());
    for (const filesystem::mojom::FileOpenResultPtr& result : results)
      files->push_back(std::move(result->file_handle));

    return true;
  }

  bool List(const base::FilePath& path,
            std::vector<base::FilePath>* const files,
            std::vector<base::FilePath>* const subdirs) override {
    DCHECK(!path.IsAbsolute());
    DCHECK(files);
    DCHECK(subdirs);

    // |dir_remote| is the directory that is open if |path| is not empty. Note
    // that it must be defined outside of the else block so it does not get
    // deleted before |dir| is used (which would make |dir| invalid).
    mojo::Remote<filesystem::mojom::Directory> dir_remote;
    filesystem::mojom::Directory* dir = nullptr;
    if (path.empty()) {
      dir = source_dir_remote_.get();
    } else {
      base::File::Error error;
      source_dir_remote_->OpenDirectory(
          path.value(), dir_remote.BindNewPipeAndPassReceiver(),
          filesystem::mojom::kFlagRead | filesystem::mojom::kFlagOpen, &error);
      if (error != base::File::Error::FILE_OK) {
        LOG(ERROR) << "Cannot open '" << path << "': Error " << error;
        return false;
      }
      dir = dir_remote.get();
    }

    absl::optional<std::vector<filesystem::mojom::DirectoryEntryPtr>> contents;
    base::File::Error error;
    dir->Read(&error, &contents);
    if (error != base::File::Error::FILE_OK) {
      LOG(ERROR) << "Cannot list content of '" << path << "': Error " << error;
      return false;
    }

    if (!contents)
      return true;

    for (const filesystem::mojom::DirectoryEntryPtr& entry : *contents) {
      (entry->type == filesystem::mojom::FsFileType::DIRECTORY ? subdirs
                                                               : files)
          ->push_back(path.Append(entry->name));
    }

    return true;
  }

  bool GetInfo(const base::FilePath& path, Info* const info) override {
    DCHECK(!path.IsAbsolute());
    DCHECK(info);

    base::File::Error error;
    filesystem::mojom::FileInformationPtr file_info;
    source_dir_remote_->StatFile(path.value(), &error, &file_info);
    if (error != base::File::Error::FILE_OK) {
      LOG(ERROR) << "Cannot get info of '" << path << "': Error " << error;
      return false;
    }

    info->is_directory =
        file_info->type == filesystem::mojom::FsFileType::DIRECTORY;
    info->last_modified = base::Time::FromDoubleT(file_info->mtime);
    return true;
  }

 private:
  // Interface ptr to the actual interface implementation used to access files.
  const mojo::Remote<filesystem::mojom::Directory> source_dir_remote_;

  DISALLOW_COPY_AND_ASSIGN(MojoFileAccessor);
};

}  // namespace

ZipFileCreator::ZipFileCreator() = default;

ZipFileCreator::~ZipFileCreator() = default;

void ZipFileCreator::CreateZipFile(
    mojo::PendingRemote<filesystem::mojom::Directory> source_dir_remote,
    const std::vector<base::FilePath>& source_relative_paths,
    base::File zip_file,
    CreateZipFileCallback callback) {
  DCHECK(zip_file.IsValid());

  for (const base::FilePath& path : source_relative_paths) {
    if (path.IsAbsolute() || path.ReferencesParent()) {
      // Paths are expected to be relative. If there are not, the API is used
      // incorrectly and this is an error.
      std::move(callback).Run(/*success=*/false);
      return;
    }
  }

  MojoFileAccessor file_accessor(std::move(source_dir_remote));
  const bool success = zip::Zip({
      .file_accessor = &file_accessor,
      .dest_fd = zip_file.GetPlatformFile(),
      .src_files = source_relative_paths,
      .progress_callback =
          base::BindRepeating([](const zip::Progress& progress) {
            VLOG(1) << "ZIP progress: " << progress;
            return true;
          }),
      .progress_period = base::TimeDelta::FromMilliseconds(500),
      .recursive = true,
  });
  std::move(callback).Run(success);
}

}  // namespace chrome
