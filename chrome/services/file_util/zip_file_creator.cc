// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/zip_file_creator.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
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
      const base::FilePath& source_dir,
      mojo::PendingRemote<filesystem::mojom::Directory> source_dir_remote)
      : source_dir_(source_dir),
        source_dir_remote_(std::move(source_dir_remote)) {}
  ~MojoFileAccessor() override = default;

 private:
  struct FileInfo {
    FileInfo() {}
    FileInfo(bool is_directory, uint64_t last_modified_time)
        : is_directory(is_directory), last_modified_time(last_modified_time) {}
    bool is_directory = false;
    uint64_t last_modified_time = 0;
  };

  std::vector<base::File> OpenFilesForReading(
      const std::vector<base::FilePath>& paths) override {
    std::vector<filesystem::mojom::FileOpenDetailsPtr> details;
    details.reserve(paths.size());
    for (const auto& path : paths) {
      filesystem::mojom::FileOpenDetailsPtr open_details(
          filesystem::mojom::FileOpenDetails::New());
      open_details->path = GetRelativePath(path).value();
      open_details->open_flags =
          filesystem::mojom::kFlagOpen | filesystem::mojom::kFlagRead;
      details.push_back(std::move(open_details));
    }
    std::vector<filesystem::mojom::FileOpenResultPtr> results;
    if (!source_dir_remote_->OpenFileHandles(std::move(details), &results))
      return std::vector<base::File>();

    std::vector<base::File> files;
    for (const auto& file_open_result : results)
      files.push_back(std::move(file_open_result->file_handle));
    return files;
  }

  bool DirectoryExists(const base::FilePath& path) override {
    const FileInfo& file_info = GetFileInfo(path);
    return file_info.is_directory;
  }

  std::vector<DirectoryContentEntry> ListDirectoryContent(
      const base::FilePath& dir_path) override {
    DCHECK(dir_path.IsAbsolute());

    std::vector<DirectoryContentEntry> results;
    // |dir_remote| is the  directory that is open if |dir_path| is not the
    // source dir. Note that it must be defined outside of the else block so it
    // does not get deleted before |dir| is used (which would make |dir|
    // invalid).
    mojo::Remote<filesystem::mojom::Directory> dir_remote;
    filesystem::mojom::Directory* dir = nullptr;
    if (source_dir_ == dir_path) {
      dir = source_dir_remote_.get();
    } else {
      base::FilePath relative_path = GetRelativePath(dir_path);
      base::File::Error error;
      source_dir_remote_->OpenDirectory(
          relative_path.value(), dir_remote.BindNewPipeAndPassReceiver(),
          filesystem::mojom::kFlagRead | filesystem::mojom::kFlagOpen, &error);
      if (error != base::File::Error::FILE_OK) {
        LOG(ERROR) << "Failed to open " << dir_path.value() << " error "
                   << error;
        return results;
      }
      dir = dir_remote.get();
    }

    base::Optional<std::vector<filesystem::mojom::DirectoryEntryPtr>>
        directory_contents;
    base::File::Error error;
    dir->Read(&error, &directory_contents);
    if (error != base::File::Error::FILE_OK) {
      LOG(ERROR) << "Failed to list content of " << dir_path.value()
                 << " error " << error;
      return results;
    }
    if (directory_contents) {
      results.reserve(directory_contents->size());
      for (const filesystem::mojom::DirectoryEntryPtr& entry :
           *directory_contents) {
        base::FilePath path = dir_path.Append(entry->name);
        bool is_directory =
            entry->type == filesystem::mojom::FsFileType::DIRECTORY;
        results.push_back(DirectoryContentEntry(path, is_directory));
      }
    }
    return results;
  }

  base::Time GetLastModifiedTime(const base::FilePath& path) override {
    const FileInfo& file_info = GetFileInfo(path);
    return base::Time::FromDoubleT(file_info.last_modified_time);
  }

  FileInfo GetFileInfo(const base::FilePath& absolute_path) {
    base::FilePath relative_path = GetRelativePath(absolute_path);
    base::File::Error error;
    filesystem::mojom::FileInformationPtr file_info_mojo;
    source_dir_remote_->StatFile(relative_path.value(), &error,
                                 &file_info_mojo);
    if (error != base::File::Error::FILE_OK) {
      LOG(ERROR) << "Failed to get last modified time of "
                 << absolute_path.value() << " error " << error;
      return FileInfo();
    }
    return FileInfo(
        file_info_mojo->type == filesystem::mojom::FsFileType::DIRECTORY,
        file_info_mojo->mtime);
  }

  base::FilePath GetRelativePath(const base::FilePath& path) {
    DCHECK(path.IsAbsolute());
    base::FilePath relative_path;
    bool success = source_dir_.AppendRelativePath(path, &relative_path);
    DCHECK(success);
    return relative_path;
  }

  // Path to the directory from which files are accessed.
  base::FilePath source_dir_;

  // Interface ptr to the actual interface implementation used to access files.
  mojo::Remote<filesystem::mojom::Directory> source_dir_remote_;

  DISALLOW_COPY_AND_ASSIGN(MojoFileAccessor);
};

}  // namespace

ZipFileCreator::ZipFileCreator() = default;

ZipFileCreator::~ZipFileCreator() = default;

void ZipFileCreator::CreateZipFile(
    mojo::PendingRemote<filesystem::mojom::Directory> source_dir_remote,
    const base::FilePath& source_dir,
    const std::vector<base::FilePath>& source_relative_paths,
    base::File zip_file,
    CreateZipFileCallback callback) {
  DCHECK(zip_file.IsValid());

  for (const auto& path : source_relative_paths) {
    if (path.IsAbsolute() || path.ReferencesParent()) {
      // Paths are expected to be relative. If there are not, the API is used
      // incorrectly and this is an error.
      std::move(callback).Run(/*success=*/false);
      return;
    }
  }

  zip::ZipParams zip_params(source_dir, zip_file.GetPlatformFile());
  zip_params.set_files_to_zip(source_relative_paths);
  zip_params.set_file_accessor(std::make_unique<MojoFileAccessor>(
      source_dir, std::move(source_dir_remote)));
  bool success = zip::Zip(zip_params);
  std::move(callback).Run(success);
}

}  // namespace chrome
