// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_ZIP_FILE_CREATOR_H_
#define CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_ZIP_FILE_CREATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "chrome/services/file_util/public/mojom/zip_file_creator.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

// ZipFileCreator creates a ZIP file from a specified list of files and
// directories under a common parent directory. This is done in a sandboxed
// utility process to protect the browser process from handling arbitrary
// input data from untrusted sources.
class ZipFileCreator {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;

  // Creates a zip file from the specified list of files and directories.
  ZipFileCreator(ResultCallback callback,
                 base::FilePath src_dir,
                 std::vector<base::FilePath> src_relative_paths,
                 base::FilePath dest_file);

  ~ZipFileCreator();

  // Starts creating the ZIP file. The result will be passed to |callback|.
  void Start(mojo::PendingRemote<chrome::mojom::FileUtilService> service);

  // Stops creating the ZIP file. Calls the |callback| with false.
  void Stop();

 private:
  // Called after the dest_file |file| is opened on the blocking pool to
  // create the zip file in it using a sandboxed utility process.
  void CreateZipFile(
      mojo::PendingRemote<chrome::mojom::FileUtilService> service,
      base::File file);

  // Notifies by calling |callback| specified in the constructor the end of the
  // ZIP operation. Deletes this.
  void ReportDone(bool success);

  // The callback.
  ResultCallback callback_;

  // The source directory for input files.
  const base::FilePath src_dir_;

  // The list of source files paths to be included in the zip file.
  // Entries are relative paths under directory |src_dir_|.
  const std::vector<base::FilePath> src_relative_paths_;

  scoped_refptr<base::SequencedTaskRunner> directory_task_runner_;

  // The output zip file.
  const base::FilePath dest_file_;

  // Remote interfaces to the file util service. Only used from the UI thread.
  mojo::Remote<chrome::mojom::FileUtilService> service_;
  mojo::Remote<chrome::mojom::ZipFileCreator> remote_zip_file_creator_;

  DISALLOW_COPY_AND_ASSIGN(ZipFileCreator);
};

#endif  // CHROME_SERVICES_FILE_UTIL_PUBLIC_CPP_ZIP_FILE_CREATOR_H_
