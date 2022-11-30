// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/xz_file_extractor.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <array>
#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/lzma_sdk/C/7zCrc.h"
#include "third_party/lzma_sdk/C/Xz.h"
#include "third_party/lzma_sdk/C/XzCrc64.h"

namespace {

constexpr int kXzBufferSize = 8192;

// XzState takes XZ data from a consumer and writes back extracted data to a
// producer. The lifecycle is managed by itself.
class XzState {
 public:
  XzState(mojo::ScopedDataPipeConsumerHandle consumer,
          mojo::ScopedDataPipeProducerHandle producer,
          XzFileExtractor::ExtractCallback success_callback)
      : consumer_(std::move(consumer)),
        producer_(std::move(producer)),
        success_callback_(std::move(success_callback)) {
    alloc_.Alloc = [](ISzAllocPtr, size_t size) { return malloc(size); };
    alloc_.Free = [](ISzAllocPtr, void* ptr) { return free(ptr); };
    XzUnpacker_Construct(&state_, &alloc_);

    consumer_watcher_.Watch(
        consumer_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&XzState::OnReadable, base::Unretained(this)));
    producer_watcher_.Watch(
        producer_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        base::BindRepeating(&XzState::OnWritable, base::Unretained(this)));
  }

 private:
  ~XzState() { XzUnpacker_Free(&state_); }

  void OnReadable(MojoResult result) { ExtractChunk(); }

  void OnWritable(MojoResult result) { ExtractChunk(); }

  void ExtractChunk() {
    MojoResult result;

    // Try to fill the buffer if it is not full.
    if (buffer_size_ < buffer_.size()) {
      uint32_t bytes_read = buffer_.size() - buffer_size_;
      result = consumer_->ReadData(buffer_.data() + buffer_size_, &bytes_read,
                                   MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        return;
      }
      if (result == MOJO_RESULT_OK) {
        buffer_size_ += bytes_read;
      } else if (result == MOJO_RESULT_FAILED_PRECONDITION) {
        // If it is the end of the input and the buffer is empty, return success
        // depending on whether it is also the end of the XZ stream.
        if (buffer_size_ == 0) {
          RunCallbackAndDeleteThis(is_end_of_stream_);
          return;
        }
        // Otherwise, process the remaining data first before finishing.
      } else {
        RunCallbackAndDeleteThis(false);
        return;
      }
    }

    ECoderStatus status = CODER_STATUS_NOT_FINISHED;

    // With mojo buffer size, decompressed data cannot always be written at
    // once. Repeat unpack and write while XzUnpacker_Code returns
    // CODER_STATUS_NOT_FINISHED.
    while (status == CODER_STATUS_NOT_FINISHED) {
      uint8_t* data = nullptr;
      uint32_t size = base::checked_cast<uint32_t>(buffer_size_);
      result = producer_->BeginWriteData(reinterpret_cast<void**>(&data), &size,
                                         MOJO_WRITE_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        continue;
      }
      if (result != MOJO_RESULT_OK) {
        RunCallbackAndDeleteThis(false);
        return;
      }

      size_t in_remaining = buffer_size_;
      size_t out_remaining = size;
      int xz_result = XzUnpacker_Code(
          &state_, data, &out_remaining, buffer_.data(), &in_remaining,
          /*srcFinished=*/buffer_size_ == 0, CODER_FINISH_ANY, &status);
      if (xz_result != SZ_OK) {
        producer_->EndWriteData(0);
        RunCallbackAndDeleteThis(false);
        return;
      }
      std::array<uint8_t, kXzBufferSize> copy_buffer;
      std::copy(buffer_.begin() + in_remaining, buffer_.begin() + buffer_size_,
                copy_buffer.begin());
      std::swap(buffer_, copy_buffer);
      buffer_size_ -= in_remaining;

      result = producer_->EndWriteData(out_remaining);
      if (result != MOJO_RESULT_OK) {
        RunCallbackAndDeleteThis(false);
        return;
      }

      is_end_of_stream_ = (status == CODER_STATUS_FINISHED_WITH_MARK ||
                           XzUnpacker_IsStreamWasFinished(&state_));
    }
  }

  void RunCallbackAndDeleteThis(bool success) {
    auto success_callback = std::move(success_callback_);
    delete this;
    std::move(success_callback).Run(success);
  }

  mojo::ScopedDataPipeConsumerHandle consumer_;
  mojo::ScopedDataPipeProducerHandle producer_;

  mojo::SimpleWatcher consumer_watcher_{
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC};
  mojo::SimpleWatcher producer_watcher_{
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC};

  XzFileExtractor::ExtractCallback success_callback_;

  CXzUnpacker state_;
  ISzAlloc alloc_;

  size_t buffer_size_ = 0;
  bool is_end_of_stream_ = false;

  std::array<uint8_t, kXzBufferSize> buffer_;
};

}  // namespace

XzFileExtractor::XzFileExtractor() {
  [[maybe_unused]] static const bool initialized = []() {
    CrcGenerateTable();
    Crc64GenerateTable();
    return true;
  }();
}

XzFileExtractor::~XzFileExtractor() = default;

void XzFileExtractor::Extract(
    mojo::ScopedDataPipeConsumerHandle xz_stream,
    mojo::ScopedDataPipeProducerHandle extracted_stream,
    XzFileExtractor::ExtractCallback callback) {
  new XzState(std::move(xz_stream), std::move(extracted_stream),
              std::move(callback));
}
