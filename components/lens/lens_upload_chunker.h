// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_UPLOAD_CHUNKER_H_
#define COMPONENTS_LENS_LENS_UPLOAD_CHUNKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/task_runner.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "url/gurl.h"

namespace endpoint_fetcher {
struct EndpointResponse;
}  // namespace endpoint_fetcher

namespace lens {

// Manages chunking, compression, progress tracking, and retries for uploading
// large files (like PDFs) to the Lens/Contextual Tasks server.
class LensUploadChunker {
 public:
  enum class ErrorType {
    kCompressionFailed,
    kNetworkError,
    kRetriesExhausted,
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Requests the delegate to upload an individual chunk.
    // The delegate is responsible for fetching OAuth tokens, creating the
    // EndpointFetcher with its own NetworkTrafficAnnotationTag, and making the
    // network call. The delegate must invoke `progress_callback` to report
    // progress updates, and `completion_callback` with the response metadata
    // once the network call returns.
    virtual void UploadChunk(
        const lens::LensOverlayUploadChunkRequest& request,
        base::RepeatingCallback<void(uint64_t position, uint64_t total)>
            progress_callback,
        base::OnceCallback<
            void(std::unique_ptr<endpoint_fetcher::EndpointResponse>)>
            completion_callback) = 0;

    // Invoked when all chunk uploads have completed successfully and the final
    // page content payload (referencing the uploaded chunks) is ready to be
    // sent.
    virtual void OnPageContentPayloadReady(
        const lens::LensOverlayRequestId& request_id,
        lens::Payload payload) = 0;

    // Invoked when a non-recoverable error occurs in the chunking or upload
    // process.
    virtual void OnChunkUploadError(ErrorType error_type) = 0;

    // Invoked when the total upload progress (across all chunks) is updated.
    virtual void OnUploadProgress(uint64_t position, uint64_t total) = 0;

    // Creates and returns the client context proto for inclusion in chunk
    // requests.
    virtual lens::LensOverlayClientContext GetClientContext() = 0;
  };

  LensUploadChunker(Delegate* delegate,
                    scoped_refptr<base::TaskRunner> compression_task_runner);
  ~LensUploadChunker();

  // Initiates the chunking and upload sequence. Splits the input bytes into
  // chunks, runs compression asynchronously, and begins uploading them via the
  // delegate.
  void Start(const lens::LensOverlayRequestId& request_id,
             lens::MimeType primary_content_type,
             const GURL& page_url,
             const std::optional<std::string>& page_title,
             base::span<const uint8_t> page_bytes);

  // Invoked when the host query controller receives a response from the objects
  // request. Parses the response for missing chunk errors. If one is found and
  // retries are available, initiates missing chunk retries and returns true
  // (telling the host to wait). Returns false otherwise.
  bool HandlePageContentResponse(const std::string& response_bytes);

  // Cancels any running compression or upload tasks and resets the internal
  // state.
  void Reset();

  uint64_t total_progress_for_testing() const { return total_progress_; }
  uint64_t total_upload_size_for_testing() const { return total_upload_size_; }

 private:
  void OnChunksCompressed(const lens::LensOverlayRequestId& request_id,
                          std::vector<std::string> compressed_chunks);
  void FetchChunkUpload(size_t chunk_index);
  void OnChunkUploadResponse(
      const lens::LensOverlayRequestId& request_id,
      size_t chunk_index,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);
  void OnChunkUploadProgress(size_t chunk_index,
                             uint64_t position,
                             uint64_t total);
  void RetryChunks(const std::vector<int64_t>& chunk_ids);

  const raw_ptr<Delegate> delegate_;
  scoped_refptr<base::TaskRunner> compression_task_runner_;
  std::unique_ptr<base::CancelableTaskTracker> compression_task_tracker_;

  lens::LensOverlayRequestId request_id_;
  lens::MimeType primary_content_type_;
  GURL page_url_;
  std::optional<std::string> page_title_;

  std::vector<std::string> chunks_;
  std::vector<uint64_t> chunk_progresses_;
  uint64_t total_progress_ = 0;
  uint64_t total_upload_size_ = 0;

  size_t remaining_responses_ = 0;
  size_t remaining_retries_ = 0;
  bool is_retry_upload_ = false;

  base::WeakPtrFactory<LensUploadChunker> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_UPLOAD_CHUNKER_H_
