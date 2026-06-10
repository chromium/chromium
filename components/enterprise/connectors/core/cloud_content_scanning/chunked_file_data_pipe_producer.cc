// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/chunked_file_data_pipe_producer.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/system/file_data_source.h"

namespace enterprise_connectors {

class ChunkedFileDataPipeProducer::BackgroundReader {
 public:
  BackgroundReader(
      base::File file,
      bool is_obfuscated,
      std::optional<enterprise_obfuscation::HeaderData> header_data) {
    if (is_obfuscated) {
      CHECK(header_data.has_value());
      auto reader = enterprise_obfuscation::ObfuscatedFileReader::Create(
          header_data.value(), std::move(file));
      if (reader.has_value()) {
        obfuscated_reader_ = std::move(reader.value());
      }
    } else {
      file_data_source_ =
          std::make_unique<mojo::FileDataSource>(std::move(file));
    }
  }

  ~BackgroundReader() = default;

  std::pair<std::vector<uint8_t>, MojoResult> ReadNextChunk(int64_t offset) {
    std::vector<uint8_t> buffer(ChunkedFileDataPipeProducer::kChunkSize);
    int64_t bytes_read = 0;

    if (obfuscated_reader_) {
      if (obfuscated_reader_->Seek(offset, base::File::FROM_BEGIN) < 0) {
        return {{}, MOJO_RESULT_UNKNOWN};
      }
      bytes_read =
          obfuscated_reader_->Read(base::as_writable_bytes(base::span(buffer)));
    } else if (file_data_source_) {
      mojo::DataPipeProducer::DataSource::ReadResult res =
          file_data_source_->Read(offset,
                                  base::as_writable_chars(base::span(buffer)));
      if (res.result != MOJO_RESULT_OK) {
        return {{}, res.result};
      }
      bytes_read = res.bytes_read;
    } else {
      return {{}, MOJO_RESULT_UNKNOWN};
    }

    if (bytes_read < 0) {
      return {{}, MOJO_RESULT_UNKNOWN};
    }
    buffer.resize(bytes_read);
    return {std::move(buffer), MOJO_RESULT_OK};
  }

 private:
  // Only one of `file_data_source_` and `obfuscated_reader_` is non-null.
  std::unique_ptr<mojo::FileDataSource> file_data_source_;
  std::optional<enterprise_obfuscation::ObfuscatedFileReader>
      obfuscated_reader_;
};

ChunkedFileDataPipeProducer::ChunkedFileDataPipeProducer(
    base::File file,
    bool is_obfuscated,
    int64_t file_size,
    std::optional<enterprise_obfuscation::HeaderData> header_data)
    : background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      file_size_(file_size) {
  background_reader_ = std::make_unique<BackgroundReader>(
      std::move(file), is_obfuscated, std::move(header_data));
}

ChunkedFileDataPipeProducer::~ChunkedFileDataPipeProducer() {
  background_task_runner_->DeleteSoon(FROM_HERE, std::move(background_reader_));
}

void ChunkedFileDataPipeProducer::ReadNextChunk(int64_t offset,
                                                ReadCallback callback) {
  DCHECK(!is_reading_);
  DCHECK(!file_fully_read_);
  is_reading_ = true;

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&BackgroundReader::ReadNextChunk,
                     base::Unretained(background_reader_.get()), offset),
      base::BindOnce(&ChunkedFileDataPipeProducer::OnReadComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChunkedFileDataPipeProducer::OnReadComplete(
    ReadCallback callback,
    std::pair<std::vector<uint8_t>, MojoResult> result) {
  is_reading_ = false;
  if (result.first.empty()) {
    file_fully_read_ = true;
  }
  std::move(callback).Run(std::move(result.first), result.second);
}

void ChunkedFileDataPipeProducer::Reset() {
  weak_factory_.InvalidateWeakPtrs();
  is_reading_ = false;
  file_fully_read_ = false;
}

}  // namespace enterprise_connectors
