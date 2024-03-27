// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
const size_t kBurningBlockSize = 1 << 20;  // 1 MB
const size_t kMemoryAlignment = 4096;

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
  for (HANDLE handle : volume_handles_) {
    CloseHandle(handle);
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
    DCHECK_EQ(0u, bytes_to_write % kMemoryAlignment);
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

  auto image_buffer = base::HeapArray<uint8_t>::WithSize(kBurningBlockSize);
  // DASD buffers require memory alignment on some systems.
  std::unique_ptr<uint8_t, base::AlignedFreeDeleter> device_buffer_ptr(
      static_cast<uint8_t*>(
          base::AlignedAlloc(kBurningBlockSize, kMemoryAlignment)));
  base::span<uint8_t> device_buffer(device_buffer_ptr.get(), kBurningBlockSize);

  std::optional<size_t> image_bytes_read =
      image_file_.Read(bytes_processed_, image_buffer);

  if (!image_bytes_read) {
    // Unable to read entire file.
    LOG(ERROR) << "Failed to read " << kBurningBlockSize << " bytes of image "
               << "at offset " << bytes_processed_;
    Error(error::kReadImage);
  } else if (image_bytes_read.value() == 0) {
    // End of file.
    handler_->SendSucceeded();
    running_ = false;
  } else {
    std::optional<size_t> device_bytes_read =
        device_file_.Read(bytes_processed_, device_buffer);
    if (!device_bytes_read ||
        device_bytes_read.value() < image_bytes_read.value()) {
      LOG(ERROR) << "Failed to read " << image_bytes_read.value()
                 << " bytes of device at offset " << bytes_processed_;
      Error(error::kReadDevice);
      return;
    }

    if (image_buffer.first(image_bytes_read.value()) !=
        device_buffer.first(device_bytes_read.value())) {
      LOG(ERROR) << "Write verification failed when comparing "
                 << image_bytes_read.value() << " bytes at "
                 << bytes_processed_;
      Error(error::kVerificationFailed);
      return;
    }

    bytes_processed_ += image_bytes_read.value();
    PostProgress(bytes_processed_);

    PostTask(base::BindOnce(&ImageWriter::VerifyChunk, AsWeakPtr()));
  }
}

}  // namespace image_writer
