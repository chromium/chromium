// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/image_writer/image_writer.h"

#include <string.h>

#include <algorithm>
#include <optional>

#include "base/compiler_specific.h"
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

namespace {

// Since block devices like large sequential access and IPC is expensive we're
// doing work in 1MB chunks.
constexpr size_t kBurningBlockSize = 1 << 20;  // 1 MB
constexpr size_t kMemoryAlignment = 4096;

}  // namespace

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
  base::AlignedHeapArray<uint8_t> buffer =
      base::AlignedUninit<uint8_t>(kBurningBlockSize, kMemoryAlignment);
  std::ranges::fill(buffer, 0);

  const std::optional<size_t> bytes_read =
      image_file_.Read(bytes_processed_, buffer.as_span());
  if (!bytes_read) {
    Error(error::kReadImage);
    return;
  }

  if (*bytes_read == 0) {
    // End of file.
    device_file_.Flush();
    running_ = false;
    handler_->SendSucceeded();
    return;
  }

  // Always attempt to write a whole block, as writing DASD requires sector-
  // aligned writes to devices.
  const size_t bytes_to_write = *bytes_read + (kMemoryAlignment - 1) -
                                (*bytes_read - 1) % kMemoryAlignment;
  DCHECK_EQ(0u, bytes_to_write % kMemoryAlignment);
  std::optional<size_t> bytes_written =
      device_file_.Write(bytes_processed_, buffer.first(bytes_to_write));

  if (!bytes_written || *bytes_written < *bytes_read) {
    Error(error::kWriteImage);
    return;
  }

  bytes_processed_ += base::checked_cast<int64_t>(*bytes_read);
  PostProgress(bytes_processed_);

  PostTask(base::BindOnce(&ImageWriter::WriteChunk, AsWeakPtr()));
}

void ImageWriter::VerifyChunk() {
  if (!IsRunning()) {
    return;
  }

  auto image_buffer = base::HeapArray<uint8_t>::Uninit(kBurningBlockSize);
  // DASD buffers require memory alignment on some systems.
  base::AlignedHeapArray<uint8_t> device_buffer =
      base::AlignedUninit<uint8_t>(kBurningBlockSize, kMemoryAlignment);

  const std::optional<size_t> bytes_read =
      image_file_.Read(bytes_processed_, image_buffer.as_span());
  if (!bytes_read) {
    // Unable to read entire file.
    LOG(ERROR) << "Failed to read " << kBurningBlockSize << " bytes of image "
               << "at offset " << bytes_processed_;
    Error(error::kReadImage);
    return;
  }

  if (*bytes_read == 0) {
    // End of file.
    handler_->SendSucceeded();
    running_ = false;
    return;
  }

  if (device_file_.Read(bytes_processed_, device_buffer.as_span()) <
      *bytes_read) {
    LOG(ERROR) << "Failed to read " << *bytes_read << " bytes of "
               << "device at offset " << bytes_processed_;
    Error(error::kReadDevice);
    return;
  }

  if (image_buffer.first(*bytes_read) != device_buffer.first(*bytes_read)) {
    LOG(ERROR) << "Write verification failed when comparing " << *bytes_read
               << " bytes at " << bytes_processed_;
    Error(error::kVerificationFailed);
    return;
  }

  bytes_processed_ += base::checked_cast<int64_t>(*bytes_read);
  PostProgress(bytes_processed_);

  PostTask(base::BindOnce(&ImageWriter::VerifyChunk, AsWeakPtr()));
}

}  // namespace image_writer
