// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_FILE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_FILE_HANDLER_H_

#include <stdint.h>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-forward.h"

// This class manages async File IO for Nearby Share file payloads. Opening and
// releasing files need to run on a MayBlock task runner.
class NearbyFileHandler {
 public:
  struct FileInfo {
    int64_t size;
    base::File file;
  };
  struct CreateFileResult {
    base::File input_file;
    base::File output_file;
  };
  using PayloadPtr = nearby::connections::mojom::PayloadPtr;
  using OpenFilesCallback = base::OnceCallback<void(std::vector<FileInfo>)>;
  using CreateFileCallback = base::OnceCallback<void(CreateFileResult)>;
  using GetUniquePathCallback = base::OnceCallback<void(base::FilePath)>;

  NearbyFileHandler();
  ~NearbyFileHandler();

  // Opens the files given in |file_paths| and returns the opened files and
  // their sizes via |callback|. If any file failed to open this will return an
  // empty list.
  void OpenFiles(std::vector<base::FilePath> file_paths,
                 OpenFilesCallback callback);

  // Releases the file |payloads| on a MayBlock task runner as closing a file
  // might block.
  void ReleaseFilePayloads(std::vector<PayloadPtr> payloads);

  // Create and open the file given in |file_path| and returns the opened files
  // via |callback|.
  void CreateFile(const base::FilePath& file_path, CreateFileCallback callback);

  void DeleteFilesFromDisk(std::vector<base::FilePath> file_paths);

  // Finds a unique path name for |file_path| and runs |callback| with the same.
  void GetUniquePath(const base::FilePath& file_path,
                     GetUniquePathCallback callback);

 private:
  // Task runner for doing file operations.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_FILE_HANDLER_H_
