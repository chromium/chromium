// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/zip_file_creator.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "third_party/zlib/google/zip.h"

namespace chrome {
namespace {

// Output operator for logging.
std::ostream& operator<<(std::ostream& out, const base::File::Error error) {
  switch (error) {
    case base::File::FILE_OK:
      return out << "FILE_OK";
#define ENTRY(S)      \
  case base::File::S: \
    return out << #S;
      ENTRY(FILE_ERROR_FAILED);
      ENTRY(FILE_ERROR_IN_USE);
      ENTRY(FILE_ERROR_EXISTS);
      ENTRY(FILE_ERROR_NOT_FOUND);
      ENTRY(FILE_ERROR_ACCESS_DENIED);
      ENTRY(FILE_ERROR_TOO_MANY_OPENED);
      ENTRY(FILE_ERROR_NO_MEMORY);
      ENTRY(FILE_ERROR_NO_SPACE);
      ENTRY(FILE_ERROR_NOT_A_DIRECTORY);
      ENTRY(FILE_ERROR_INVALID_OPERATION);
      ENTRY(FILE_ERROR_SECURITY);
      ENTRY(FILE_ERROR_ABORT);
      ENTRY(FILE_ERROR_NOT_A_FILE);
      ENTRY(FILE_ERROR_NOT_EMPTY);
      ENTRY(FILE_ERROR_INVALID_URL);
      ENTRY(FILE_ERROR_IO);
#undef ENTRY
    default:
      return out << "File::Error("
                 << static_cast<std::underlying_type_t<base::File::Error>>(
                        error)
                 << ")";
  }
}

std::string Redact(const std::string& s) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", s, "'"}) : "(redacted)";
}

std::string Redact(const base::FilePath& path) {
  return Redact(path.value());
}

// A zip::FileAccessor that talks to a file system through the Mojo
// filesystem::mojom::Directory.
class MojoFileAccessor : public zip::FileAccessor {
 public:
  explicit MojoFileAccessor(
      mojo::PendingRemote<filesystem::mojom::Directory> src_dir)
      : src_dir_(std::move(src_dir)) {}

  MojoFileAccessor(const MojoFileAccessor&) = delete;
  MojoFileAccessor& operator=(const MojoFileAccessor&) = delete;

  ~MojoFileAccessor() override = default;

  bool Open(const zip::Paths paths,
            std::vector<base::File>* const files) override {
    DCHECK(!paths.empty());

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
    if (!src_dir_->OpenFileHandles(std::move(details), &results)) {
      LOG(ERROR) << "Cannot open " << Redact(paths.front()) << " and "
                 << (paths.size() - 1) << " other files";
      return false;
    }

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
      dir = src_dir_.get();
    } else {
      base::File::Error error;
      src_dir_->OpenDirectory(
          path.value(), dir_remote.BindNewPipeAndPassReceiver(),
          filesystem::mojom::kFlagRead | filesystem::mojom::kFlagOpen, &error);
      if (error != base::File::Error::FILE_OK) {
        LOG(ERROR) << "Cannot open " << Redact(path) << ": " << error;
        return false;
      }
      dir = dir_remote.get();
    }

    std::optional<std::vector<filesystem::mojom::DirectoryEntryPtr>> contents;
    base::File::Error error;
    dir->Read(&error, &contents);
    if (error != base::File::Error::FILE_OK) {
      LOG(ERROR) << "Cannot list content of " << Redact(path) << ": " << error;
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
    src_dir_->StatFile(path.value(), &error, &file_info);
    if (error != base::File::Error::FILE_OK) {
      LOG(ERROR) << "Cannot stat " << Redact(path) << ": " << error;
      return false;
    }

    info->is_directory =
        file_info->type == filesystem::mojom::FsFileType::DIRECTORY;
    info->last_modified =
        base::Time::FromSecondsSinceUnixEpoch(file_info->mtime);
    return true;
  }

 private:
  // Interface ptr to the source directory.
  const mojo::Remote<filesystem::mojom::Directory> src_dir_;
};

}  // namespace

ZipFileCreator::ZipFileCreator(PendingCreator receiver)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&ZipFileCreator::OnDisconnect, base::AdoptRef(this)));
}

ZipFileCreator::~ZipFileCreator() {
  DCHECK(cancelled_.IsSet());
}

void ZipFileCreator::CreateZipFile(
    PendingDirectory src_dir,
    const std::vector<base::FilePath>& relative_paths,
    base::File zip_file,
    PendingListener listener) {
  DCHECK(zip_file.IsValid());

  for (const base::FilePath& path : relative_paths) {
    if (path.IsAbsolute() || path.ReferencesParent()) {
      // Paths are expected to be relative. If there are not, the API is used
      // incorrectly and this is an error.
      Listener(std::move(listener))->OnFinished(/*success=*/false);
      return;
    }
  }

  runner_->PostTask(
      FROM_HERE, base::BindOnce(&ZipFileCreator::WriteZipFile, this,
                                std::move(src_dir), std::move(relative_paths),
                                std::move(zip_file), std::move(listener)));
}

void ZipFileCreator::WriteZipFile(
    PendingDirectory src_dir,
    const std::vector<base::FilePath>& relative_paths,
    base::File zip_file,
    PendingListener pending_listener) const {
  MojoFileAccessor file_accessor(std::move(src_dir));
  const Listener listener(std::move(pending_listener));
  const bool success = zip::Zip({
      .file_accessor = &file_accessor,
      .dest_fd = zip_file.GetPlatformFile(),
      .src_files = relative_paths,
      .progress_callback = base::BindRepeating(&ZipFileCreator::OnProgress,
                                               this, std::cref(listener)),
      .progress_period = base::Seconds(1),
      .recursive = true,
      .continue_on_error = true,
  });

  listener->OnFinished(success);
}

bool ZipFileCreator::OnProgress(const Listener& listener,
                                const zip::Progress& progress) const {
  listener->OnProgress(progress.bytes, progress.files, progress.directories);
  return !cancelled_.IsSet();
}

void ZipFileCreator::OnDisconnect() {
  DCHECK(receiver_.is_bound());
  receiver_.reset();
  DCHECK(!cancelled_.IsSet());
  cancelled_.Set();
}

}  // namespace chrome
