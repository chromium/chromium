// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CHUNKED_FILE_DATA_PIPE_PRODUCER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CHUNKED_FILE_DATA_PIPE_PRODUCER_H_

#include <memory>
#include <optional>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/obfuscation/core/obfuscated_file_reader.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace enterprise_connectors {

// A helper class that reads files (obfuscated or regular) sequentially
// in chunks on a blockable sequence.
class ChunkedFileDataPipeProducer {
 public:
  static constexpr size_t kChunkSize = 512 * 1024;  // 512KB

  ChunkedFileDataPipeProducer(
      base::File file,
      bool is_obfuscated,
      int64_t file_size,
      std::optional<enterprise_obfuscation::HeaderData> header_data);

  ChunkedFileDataPipeProducer(const ChunkedFileDataPipeProducer&) = delete;
  ChunkedFileDataPipeProducer& operator=(const ChunkedFileDataPipeProducer&) =
      delete;

  ~ChunkedFileDataPipeProducer();

  using ReadCallback =
      base::OnceCallback<void(std::vector<uint8_t> chunk, MojoResult result)>;

  // Reads the next chunk of the file starting at `offset`.
  // Invokes `callback` on the calling sequence.
  void ReadNextChunk(int64_t offset, ReadCallback callback);

  // Resets the state of the producer (cancels any pending reads).
  void Reset();

  int64_t file_size() const { return file_size_; }

  // Returns true if `ReadNextChunk` has been called while its callback hasn't
  // returned yet.
  bool is_reading() const { return is_reading_; }

  // Return true if the file has been fully read.
  bool file_fully_read() const { return file_fully_read_; }

 private:
  class BackgroundReader;

  void OnReadComplete(ReadCallback callback,
                      std::pair<std::vector<uint8_t>, MojoResult> result);

  std::unique_ptr<BackgroundReader> background_reader_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  int64_t file_size_;

  bool is_reading_ = false;
  bool file_fully_read_ = false;

  base::WeakPtrFactory<ChunkedFileDataPipeProducer> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CHUNKED_FILE_DATA_PIPE_PRODUCER_H_
