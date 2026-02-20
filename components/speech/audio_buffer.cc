// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/speech/audio_buffer.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/compiler_specific.h"

namespace {
void VerifyBytesPerSample(int bytes_per_sample) {
  CHECK(bytes_per_sample == 1 || bytes_per_sample == 2 ||
        bytes_per_sample == 4);
}

base::AlignedHeapArray<uint8_t> MakeBackingData(size_t length,
                                                int bytes_per_sample) {
  if (!length) [[unlikely]] {
    return base::AlignedHeapArray<uint8_t>();
  }
  return base::AlignedUninit<uint8_t>(length, bytes_per_sample);
}
}  // namespace

AudioChunk::AudioChunk(int bytes_per_sample)
    : bytes_per_sample_(bytes_per_sample) {
  VerifyBytesPerSample(bytes_per_sample);
}

AudioChunk::AudioChunk(size_t length, int bytes_per_sample)
    : data_(MakeBackingData(length, bytes_per_sample)),
      bytes_per_sample_(bytes_per_sample) {
  VerifyBytesPerSample(bytes_per_sample);
  CHECK_EQ(length % bytes_per_sample, 0U);
  std::ranges::fill(data_, 0);
}

AudioChunk::AudioChunk(const uint8_t* data, size_t length, int bytes_per_sample)
    : data_(MakeBackingData(length, bytes_per_sample)),
      bytes_per_sample_(bytes_per_sample) {
  VerifyBytesPerSample(bytes_per_sample);
  CHECK_EQ(length % bytes_per_sample, 0U);
  // SAFETY: Per API contract, `data` must contain exactly `length` bytes.
  base::span(data_).copy_from_nonoverlapping(
      UNSAFE_BUFFERS(base::span(data, length)));
}

AudioChunk::AudioChunk(base::span<const uint8_t> data_span,
                       int bytes_per_sample)
    : data_(MakeBackingData(data_span.size(), bytes_per_sample)),
      bytes_per_sample_(bytes_per_sample) {
  CHECK_EQ(data_span.size() % bytes_per_sample, 0U);
  base::span(data_).copy_from_nonoverlapping(data_span);
}

AudioChunk::~AudioChunk() = default;

bool AudioChunk::IsEmpty() const {
  return data_.empty();
}

size_t AudioChunk::NumSamples() const {
  return data_.size() / bytes_per_sample_;
}

int16_t AudioChunk::GetSample16(size_t index) const {
  // `SamplesData16AsSpan()` will check all the necessary safety constraints.
  return SamplesData16AsSpan()[index];
}

std::string_view AudioChunk::AsStringView() const {
  return std::string_view(reinterpret_cast<const char*>(data_.data()),
                          data_.size());
}

std::string AudioChunk::AsString() {
  // The extra copy is unfortunate here...
  // TODO(crbug.com/484089981): Delete this method when all uses have been
  // replaced by `AsStringView()`.
  return std::string(reinterpret_cast<const char*>(data_.data()), data_.size());
}

base::span<const int16_t> AudioChunk::SamplesData16AsSpan() const {
  // SAFETY: `SamplesData16AsSpan()` returns a pointer to `data_`. Make sure
  // this is safe by ensuring the byte size is a multiple of sizeof(int16_t) and
  // the data is properly aligned (which the constructor should already
  // guarantee).
  CHECK_EQ(static_cast<size_t>(bytes_per_sample_), sizeof(int16_t));
  CHECK(base::IsAligned(data_.data(), alignof(int16_t)));
  return UNSAFE_BUFFERS(
      base::span<const int16_t>(reinterpret_cast<const int16_t*>(data_.data()),
                                data_.size() / sizeof(int16_t)));
}

base::span<int16_t> AudioChunk::SamplesData16AsWriteableSpan() {
  // SAFETY: `SamplesData16AsSpan()` returns a pointer to `data_`. Make sure
  // this is safe by ensuring the byte size is a multiple of sizeof(int16_t) and
  // the data is properly aligned (which the constructor should already
  // guarantee).
  CHECK_EQ(static_cast<size_t>(bytes_per_sample_), sizeof(int16_t));
  CHECK(base::IsAligned(data_.data(), alignof(int16_t)));
  return UNSAFE_BUFFERS(
      base::span<int16_t>(reinterpret_cast<int16_t*>(data_.data()),
                          data_.size() / sizeof(int16_t)));
}

AudioBuffer::AudioBuffer(int bytes_per_sample)
    : bytes_per_sample_(bytes_per_sample) {
  VerifyBytesPerSample(bytes_per_sample);
}

AudioBuffer::~AudioBuffer() {
  Clear();
}

void AudioBuffer::Enqueue(const uint8_t* data, size_t length) {
  chunks_.push_back(new AudioChunk(data, length, bytes_per_sample_));
}

scoped_refptr<AudioChunk> AudioBuffer::DequeueSingleChunk() {
  DCHECK(!chunks_.empty());
  scoped_refptr<AudioChunk> chunk(chunks_.front());
  chunks_.pop_front();
  return chunk;
}

scoped_refptr<AudioChunk> AudioBuffer::DequeueAll() {
  if (chunks_.empty()) {
    return base::MakeRefCounted<AudioChunk>(bytes_per_sample_);
  }

  base::CheckedNumeric<size_t> total_size = 0;
  // In order to improve performance, calculate in advance the total length
  // and then copy the chunks.
  for (const auto& chunk : chunks_) {
    total_size += chunk->data().size();
  }

  auto merged_chunk = base::MakeRefCounted<AudioChunk>(total_size.ValueOrDie(),
                                                       bytes_per_sample_);
  base::span<uint8_t> remaining_data = merged_chunk->writable_data();

  for (const auto& chunk : chunks_) {
    auto dest = remaining_data.take_first(chunk->data().size());
    dest.copy_from_nonoverlapping(chunk->data());
  }

  CHECK(remaining_data.empty());

  Clear();
  return merged_chunk;
}

void AudioBuffer::Clear() {
  chunks_.clear();
}

bool AudioBuffer::IsEmpty() const {
  return chunks_.empty();
}
