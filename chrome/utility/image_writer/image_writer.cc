// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/utility/image_writer/image_writer.h"

#include <string.h>

#include "base/containers/heap_array.h"
#include "base/location.h"
#include "base/memory/aligned_memory.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/utility/image_writer/error_message_strings.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#include "content/public/utility/utility_thread.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/utility/image_writer/disk_unmounter_mac.h"
#endif

namespace image_writer {

// Since block devices like large sequential access and IPC is expensive we're
// doing work in 1MB chunks.
const int kBurningBlockSize = 1 << 20;  // 1 MB
const int kMemoryAlignment = 4096;

ImageWriter::ImageWriter(ImageWriterHandler* handler,
                         const base::FilePath& image_path,
                         const base::FilePath& device_path)
    : image_path_(image_path),
      device_path_(device_path),
      bytes_processed_(0),
      running_(false),
      handler_(handler) {}

ImageWriter::~ImageWriter() {
#if BUILDFLAG(IS_WIN)
  for (std::vector<HANDLE>::const_iterator it = volume_handles_.begin();
       it != volume_handles_.end();
       ++it) {
    CloseHandle(*it);
  }
#endif
}

void ImageWriter::Write() {
  if (!InitializeFiles()) {
    return;
  }

  PostProgress(0);
  PostTask(base::BindOnce(&ImageWriter::WriteChunk, AsWeakPtr()));
}

void ImageWriter::Verify() {
  if (!InitializeFiles()) {
    return;
  }

  PostProgress(0);
  PostTask(base::BindOnce(&ImageWriter::VerifyChunk, AsWeakPtr()));
}

void ImageWriter::Cancel() {
  running_ = false;
  handler_->SendCancelled();
}

bool ImageWriter::IsRunning() const { return running_; }

const base::FilePath& ImageWriter::GetImagePath() { return image_path_; }

const base::FilePath& ImageWriter::GetDevicePath() { return device_path_; }

void ImageWriter::PostTask(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}

void ImageWriter::PostProgress(int64_t progress) {
  handler_->SendProgress(progress);
}

void ImageWriter::Error(const std::string& message) {
  running_ = false;
  handler_->SendFailed(message);
}

bool ImageWriter::InitializeFiles() {
  if (!image_file_.IsValid()) {
    image_file_.Initialize(image_path_,
                           base::File::FLAG_OPEN | base::File::FLAG_READ |
                               base::File::FLAG_WIN_EXCLUSIVE_READ);

    if (!image_file_.IsValid()) {
      DLOG(ERROR) << "Unable to open file for read: " << image_path_.value();
      Error(error::kOpenImage);
      return false;
    }
  }

  if (!device_file_.IsValid()) {
    if (!OpenDevice()) {
      Error(error::kOpenDevice);
      return false;
    }
  }

  bytes_processed_ = 0;
  running_ = true;

  return true;
}

void ImageWriter::WriteChunk() {
  if (!IsRunning()) {
    return;
  }

  // DASD buffers require memory alignment on some systems.
  std::unique_ptr<char, base::AlignedFreeDeleter> buffer(static_cast<char*>(
      base::AlignedAlloc(kBurningBlockSize, kMemoryAlignment)));
  memset(buffer.get(), 0, kBurningBlockSize);

  int bytes_read = image_file_.Read(bytes_processed_, buffer.get(),
                                    kBurningBlockSize);

  if (bytes_read > 0) {
    // Always attempt to write a whole block, as writing DASD requires sector-
    // aligned writes to devices.
    int bytes_to_write = bytes_read + (kMemoryAlignment - 1) -
                         (bytes_read - 1) % kMemoryAlignment;
    DCHECK_EQ(0, bytes_to_write % kMemoryAlignment);
    int bytes_written =
        device_file_.Write(bytes_processed_, buffer.get(), bytes_to_write);

    if (bytes_written < bytes_read) {
      Error(error::kWriteImage);
      return;
    }

    bytes_processed_ += bytes_read;
    PostProgress(bytes_processed_);

    PostTask(base::BindOnce(&ImageWriter::WriteChunk, AsWeakPtr()));
  } else if (bytes_read == 0) {
    // End of file.
    device_file_.Flush();
    running_ = false;
    handler_->SendSucceeded();
  } else {
    // Unable to read entire file.
    Error(error::kReadImage);
  }
}

void ImageWriter::VerifyChunk() {
  if (!IsRunning()) {
    return;
  }

  auto image_buffer = base::HeapArray<char>::Uninit(kBurningBlockSize);
  // DASD buffers require memory alignment on some systems.
  std::unique_ptr<char, base::AlignedFreeDeleter> device_buffer(
      static_cast<char*>(
          base::AlignedAlloc(kBurningBlockSize, kMemoryAlignment)));

  int bytes_read = image_file_.Read(bytes_processed_, image_buffer.data(),
                                    kBurningBlockSize);

  if (bytes_read > 0) {
    if (device_file_.Read(bytes_processed_,
                          device_buffer.get(),
                          kBurningBlockSize) < bytes_read) {
      LOG(ERROR) << "Failed to read " << bytes_read << " bytes of "
                 << "device at offset " << bytes_processed_;
      Error(error::kReadDevice);
      return;
    }

    if (memcmp(image_buffer.data(), device_buffer.get(), bytes_read) != 0) {
      LOG(ERROR) << "Write verification failed when comparing " << bytes_read
                 << " bytes at " << bytes_processed_;
      Error(error::kVerificationFailed);
      return;
    }

    bytes_processed_ += bytes_read;
    PostProgress(bytes_processed_);

    PostTask(base::BindOnce(&ImageWriter::VerifyChunk, AsWeakPtr()));
  } else if (bytes_read == 0) {
    // End of file.
    handler_->SendSucceeded();
    running_ = false;
  } else {
    // Unable to read entire file.
    LOG(ERROR) << "Failed to read " << kBurningBlockSize << " bytes of image "
               << "at offset " << bytes_processed_;
    Error(error::kReadImage);
  }
}

}  // namespace image_writer
