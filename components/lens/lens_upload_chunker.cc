// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_upload_chunker.h"

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_payload_construction.h"
#include "google_apis/common/api_error_codes.h"

namespace lens {

namespace {

// Divides the content_bytes into small chunks, which are then compressed.
std::vector<std::string> MakeChunks(std::vector<uint8_t> content_bytes) {
  base::span<const uint8_t> content_span(content_bytes);
  base::SpanReader reader(content_span);
  size_t max_chunk_size = lens::features::GetLensOverlayChunkSizeBytes();
  std::vector<std::string> chunks;

  while (reader.remaining() > 0) {
    size_t chunk_size = std::min(reader.remaining(), max_chunk_size);
    auto current_chunk = reader.Read(chunk_size);
    CHECK(current_chunk.has_value());

    std::string chunk;
    const bool success = lens::ZstdCompressBytes(current_chunk.value(), &chunk);
    if (!success) {
      // If any of the chunks fail to compress, then the request should fail.
      return std::vector<std::string>();
    }

    chunks.push_back(chunk);
  }
  return chunks;
}

// Creates the lens::LensOverlayUploadChunkRequest for the given chunk.
lens::LensOverlayUploadChunkRequest CreateUploadChunkRequest(
    int64_t chunk_id,
    int64_t total_chunks,
    const std::string& chunk,
    const lens::LensOverlayRequestContext& request_context) {
  lens::LensOverlayUploadChunkRequest request;
  request.mutable_request_context()->CopyFrom(request_context);
  request.mutable_debug_options()->set_total_chunks(total_chunks);
  request.set_chunk_id(chunk_id);
  request.mutable_chunk_bytes()->assign(chunk.begin(), chunk.end());
  return request;
}

// Returns the lens::Payload to be sent after uploading chunked data.
lens::Payload CreatePageContentPayloadForChunks(
    lens::MimeType primary_content_type,
    const GURL& page_url,
    const std::optional<std::string>& page_title,
    int64_t total_stored_chunks,
    bool is_read_retry) {
  lens::Payload payload;
  auto* content = payload.mutable_content();

  if (!page_url.is_empty()) {
    content->set_webpage_url(page_url.spec());
  }
  if (page_title.has_value() && !page_title.value().empty()) {
    content->set_webpage_title(page_title.value());
  }

  auto* content_data = content->add_content_data();
  content_data->set_content_type(
      lens::MimeTypeToContentType(primary_content_type));
  content_data->mutable_stored_chunk_options()->set_read_stored_chunks(true);
  content_data->mutable_stored_chunk_options()->set_total_stored_chunks(
      total_stored_chunks);
  content_data->mutable_stored_chunk_options()->set_is_read_retry(
      is_read_retry);
  content_data->set_compression_type(lens::CompressionType::ZSTD);
  return payload;
}

}  // namespace

LensUploadChunker::LensUploadChunker(
    Delegate* delegate,
    scoped_refptr<base::TaskRunner> compression_task_runner)
    : delegate_(delegate),
      compression_task_runner_(compression_task_runner),
      compression_task_tracker_(
          std::make_unique<base::CancelableTaskTracker>()) {
  CHECK(delegate_);
  CHECK(compression_task_runner_);
}

LensUploadChunker::~LensUploadChunker() = default;

void LensUploadChunker::Start(const lens::LensOverlayRequestId& request_id,
                              lens::MimeType primary_content_type,
                              const GURL& page_url,
                              const std::optional<std::string>& page_title,
                              base::span<const uint8_t> page_bytes) {
  Reset();

  request_id_ = request_id;
  primary_content_type_ = primary_content_type;
  page_url_ = page_url;
  page_title_ = page_title;
  remaining_retries_ = lens::features::GetLensOverlayUploadChunkRetries();

  compression_task_tracker_->PostTaskAndReplyWithResult(
      compression_task_runner_.get(), FROM_HERE,
      base::BindOnce(&MakeChunks, std::vector<uint8_t>(page_bytes.begin(),
                                                       page_bytes.end())),
      base::BindOnce(&LensUploadChunker::OnChunksCompressed,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void LensUploadChunker::OnChunksCompressed(
    const lens::LensOverlayRequestId& request_id,
    std::vector<std::string> compressed_chunks) {
  if (request_id.sequence_id() != request_id_.sequence_id()) {
    return;
  }

  if (compressed_chunks.empty()) {
    delegate_->OnChunkUploadError(ErrorType::kCompressionFailed);
    return;
  }

  chunks_ = std::move(compressed_chunks);
  chunk_progresses_ = std::vector<uint64_t>(chunks_.size(), 0);
  total_progress_ = 0;
  total_upload_size_ = 0;

  remaining_responses_ = chunks_.size();
  for (const auto & chunk : chunks_) {
    total_upload_size_ += chunk.size();
  }

  for (size_t i = 0; i < chunks_.size(); ++i) {
    FetchChunkUpload(i);
  }
}

void LensUploadChunker::FetchChunkUpload(size_t chunk_index) {
  lens::LensOverlayRequestContext request_context;
  request_context.mutable_request_id()->CopyFrom(request_id_);
  request_context.mutable_client_context()->CopyFrom(
      delegate_->GetClientContext());

  lens::LensOverlayUploadChunkRequest request = CreateUploadChunkRequest(
      chunk_index, chunks_.size(), chunks_[chunk_index], request_context);

  delegate_->UploadChunk(
      request,
      base::BindRepeating(&LensUploadChunker::OnChunkUploadProgress,
                          weak_ptr_factory_.GetWeakPtr(), chunk_index),
      base::BindOnce(&LensUploadChunker::OnChunkUploadResponse,
                     weak_ptr_factory_.GetWeakPtr(), request_id_, chunk_index));
}

void LensUploadChunker::OnChunkUploadResponse(
    const lens::LensOverlayRequestId& request_id,
    size_t chunk_index,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  if (request_id.sequence_id() != request_id_.sequence_id()) {
    return;
  }

  if (!response ||
      response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    delegate_->OnChunkUploadError(ErrorType::kNetworkError);
    return;
  }

  remaining_responses_--;
  if (remaining_responses_ == 0) {
    // All chunks uploaded! Send the objects request.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CreatePageContentPayloadForChunks,
                       primary_content_type_, page_url_, page_title_,
                       chunks_.size(), is_retry_upload_),
        base::BindOnce(&LensUploadChunker::Delegate::OnPageContentPayloadReady,
                       base::Unretained(delegate_), request_id_));
  }
}

void LensUploadChunker::OnChunkUploadProgress(size_t chunk_index,
                                              uint64_t position,
                                              uint64_t total) {
  total_progress_ += position - chunk_progresses_[chunk_index];
  chunk_progresses_[chunk_index] = position;

  if (total_progress_ > total_upload_size_) {
    total_progress_ = total_upload_size_;
  }

  delegate_->OnUploadProgress(total_progress_, total_upload_size_);
}

bool LensUploadChunker::HandlePageContentResponse(
    const std::string& response_bytes) {
  // Check if the server response contains missing chunk errors to handle.
  // Proceed without handling if out of retries.
  if (remaining_retries_ > 0) {
    lens::LensOverlayServerResponse server_response;
    bool parse_successful = server_response.ParseFromString(response_bytes);
    if (parse_successful &&
        server_response.error().error_type() ==
            LensOverlayServerError_ErrorType::
                LensOverlayServerError_ErrorType_MISSING_CHUNKS) {
      remaining_retries_--;
      auto missing_chunks_metadata =
          server_response.error().missing_chunks_metadata();
      if (!missing_chunks_metadata.has_chunk_metadata()) {
        // Interaction request likely misrouted. Resend it completely.
        is_retry_upload_ = true;
        base::SequencedTaskRunner::GetCurrentDefault()
            ->PostTaskAndReplyWithResult(
                FROM_HERE,
                base::BindOnce(&CreatePageContentPayloadForChunks,
                               primary_content_type_, page_url_, page_title_,
                               chunks_.size(), is_retry_upload_),
                base::BindOnce(
                    &LensUploadChunker::Delegate::OnPageContentPayloadReady,
                    base::Unretained(delegate_), request_id_));
        return true;
      }

      if (missing_chunks_metadata.missing_chunk_ids_size() > 0) {
        // Missing chunks. Resend only the missing chunks.
        is_retry_upload_ = true;
        std::vector<int64_t> missing_chunk_ids;
        for (int64_t chunk_id : missing_chunks_metadata.missing_chunk_ids()) {
          missing_chunk_ids.push_back(chunk_id);
        }
        RetryChunks(missing_chunk_ids);
        return true;
      }
    }
  }
  return false;
}

void LensUploadChunker::RetryChunks(const std::vector<int64_t>& chunk_ids) {
  remaining_responses_ = chunk_ids.size();
  for (int64_t chunk_id : chunk_ids) {
    if (static_cast<size_t>(chunk_id) < chunks_.size()) {
      FetchChunkUpload(static_cast<size_t>(chunk_id));
    } else {
      delegate_->OnChunkUploadError(ErrorType::kRetriesExhausted);
      return;
    }
  }
}

void LensUploadChunker::Reset() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  compression_task_tracker_->TryCancelAll();
  chunks_.clear();
  chunk_progresses_.clear();
  total_progress_ = 0;
  total_upload_size_ = 0;
  remaining_responses_ = 0;
  is_retry_upload_ = false;
}

}  // namespace lens
