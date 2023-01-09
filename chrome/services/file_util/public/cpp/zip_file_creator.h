// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_ZIP_FILE_CREATOR_H_
#define CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_ZIP_FILE_CREATOR_H_

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "chrome/services/file_util/public/mojom/zip_file_creator.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

// ZipFileCreator creates a ZIP file from a specified list of files and
// directories under a common parent directory. This is done in a sandboxed
// utility process to protect the browser process from handling arbitrary
// input data from untrusted sources.
class ZipFileCreator : public base::RefCountedThreadSafe<ZipFileCreator>,
                       private chrome::mojom::ZipListener {
 public:
  // ZIP creator result.
  // These values are persisted to logs and UMA (except kInProgress).
  // Entries should not be renumbered and numeric values should never be reused.
  enum Result {
    kInProgress = -1,  // This one is not recorded in UMA.
    kSuccess = 0,
    kCancelled = 1,
    kError = 2,
    kMaxValue = kError,
  };

  // Output operator for logging.
  friend std::ostream& operator<<(std::ostream& out, Result result);

  // Progress event of a ZIP creation operation.
  struct Progress {
    // Total number of bytes read from files getting zipped so far.
    int64_t bytes = 0;

    // Number of file entries added to the ZIP so far.
    // A file entry is added after its bytes have been processed.
    int files = 0;

    // Number of directory entries added to the ZIP so far.
    // A directory entry is added before items in it.
    int directories = 0;

    // Number of progress events received so far.
    int update_count = 0;

    // ZIP creator result.
    Result result = kInProgress;
  };

  // Creates a zip file from the specified list of files and directories.
  ZipFileCreator(base::FilePath src_dir,
                 std::vector<base::FilePath> src_relative_paths,
                 base::FilePath dest_file);

  ZipFileCreator(const ZipFileCreator&) = delete;
  ZipFileCreator& operator=(const ZipFileCreator&) = delete;

  // Sets the optional progress callback.
  // This callback will be called the next time a progress event is received.
  // Precondition: the progress callback hasn't been set yet, or the previously
  // set progress callback has already been called.
  // Precondition: this ZipFileCreator is not in its final state yet.
  void SetProgressCallback(base::OnceClosure callback);

  // Sets the optional completion callback.
  // Precondition: the completion callback hasn't been set yet.
  // Precondition: this ZipFileCreator is not in its final state yet.
  void SetCompletionCallback(base::OnceClosure callback);

  // Gets the latest progress information.
  Progress GetProgress() const { return progress_; }

  // Gets the final result.
  Result GetResult() const { return progress_.result; }

  // Starts creating the ZIP file.
  void Start(mojo::PendingRemote<chrome::mojom::FileUtilService> service);

  // Stops creating the ZIP file.
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<ZipFileCreator>;

  ~ZipFileCreator() override;

  // Called after the dest_file |file| is opened on the blocking pool to
  // create the ZIP file in it using a sandboxed utility process.
  void CreateZipFile(
      mojo::PendingRemote<chrome::mojom::FileUtilService> service,
      base::File file);

  // Binds the Directory receiver to its implementation.
  void BindDirectory(
      mojo::PendingReceiver<filesystem::mojom::Directory> receiver) const;

  // Called when the ZipFileCreator service finished.
  void OnFinished(bool success) override;

  // Notifies the end of the ZIP operation.
  void ReportResult(Result result);

  // ZIP progress report.
  void OnProgress(uint64_t bytes,
                  uint32_t files,
                  uint32_t directories) override;

  // Latest progress information.
  Progress progress_;

  // Progress callback.
  base::OnceClosure progress_callback_;

  // Final completion callback.
  base::OnceClosure completion_callback_;

  // The source directory for input files.
  const base::FilePath src_dir_;

  // The list of source files paths to be included in the zip file.
  // Entries are relative paths under directory |src_dir_|.
  const std::vector<base::FilePath> src_relative_paths_;

  // The output ZIP file path.
  const base::FilePath dest_file_;

  // Remote interfaces to the file util service. Only used from the UI thread.
  mojo::Remote<chrome::mojom::FileUtilService> service_;
  mojo::Remote<chrome::mojom::ZipFileCreator> remote_zip_file_creator_;

  // Listener receiver.
  mojo::Receiver<chrome::mojom::ZipListener> listener_{this};
};

#endif  // CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_ZIP_FILE_CREATOR_H_
