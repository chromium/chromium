// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_H_
#define CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace image_writer {

class ImageWriterHandler;
#if BUILDFLAG(IS_MAC)
class DiskUnmounterMac;
#endif

// Manages a write within the utility thread.  This class holds all the state
// around the writing and communicates with the ImageWriterHandler to dispatch
// messages.
class ImageWriter final {
 public:
  explicit ImageWriter(ImageWriterHandler* handler,
                       const base::FilePath& image_path,
                       const base::FilePath& device_path);
  ~ImageWriter();

  // Starts a write from |image_path_| to |device_path_|.
  void Write();
  // Starts verifying that |image_path_| and |device_path_| have the same size
  // and contents.
  void Verify();
  // Cancels any pending writes or verifications.
  void Cancel();

  // Returns whether an operation is in progress.
  bool IsRunning() const;
  // Checks if a path is a valid target device.
  // This method has OS-specific implementations.
  bool IsValidDevice();
  // Unmounts all volumes on the target device.
  // This method has OS-specific implementations.
  void UnmountVolumes(base::OnceClosure continuation);

  // Return the current image path.
  const base::FilePath& GetImagePath();
  // Return the current device path.
  const base::FilePath& GetDevicePath();

  base::WeakPtr<ImageWriter> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Convenience wrappers.
  void PostTask(base::OnceClosure task);
  void PostProgress(int64_t progress);
  void Error(const std::string& message);

  // Initializes the files.
  bool InitializeFiles();
  bool OpenDevice();

  // Work loops.
  void WriteChunk();
  void VerifyChunk();

  base::FilePath image_path_;
  base::FilePath device_path_;

  base::File image_file_;
  base::File device_file_;
  int64_t bytes_processed_;
  bool running_;

#if BUILDFLAG(IS_WIN)
  std::vector<HANDLE> volume_handles_;
#endif

#if BUILDFLAG(IS_MAC)
  friend class DiskUnmounterMac;
  std::unique_ptr<DiskUnmounterMac> unmounter_;
#endif

  raw_ptr<ImageWriterHandler> handler_;
  base::WeakPtrFactory<ImageWriter> weak_ptr_factory_{this};
};

}  // namespace image_writer

#endif  // CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_H_
