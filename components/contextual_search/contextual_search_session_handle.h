// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "mojo/public/cpp/base/big_buffer.h"

class GURL;

namespace lens {
enum class MimeType;
struct ContextualInputData;
namespace proto {
class LensOverlaySuggestInputs;
}  // namespace proto
}  // namespace lens

namespace contextual_search {
using SessionId = base::UnguessableToken;
class ContextualSearchService;
using AddFileContextCallback =
    base::OnceCallback<void(const ::base::UnguessableToken&)>;

// RAII handle for managing the lifetime of a ComposeboxQueryController.
class ContextualSearchSessionHandle {
 public:
  ContextualSearchSessionHandle(const ContextualSearchSessionHandle&) = delete;
  ContextualSearchSessionHandle& operator=(
      const ContextualSearchSessionHandle&) = delete;
  ContextualSearchSessionHandle(ContextualSearchSessionHandle&&) = delete;
  ContextualSearchSessionHandle& operator=(ContextualSearchSessionHandle&&) =
      delete;
  ~ContextualSearchSessionHandle();

  base::UnguessableToken session_id() const { return session_id_; }

  // Returns the ContextualSearchContextController reference held by this
  // handle or nullptr if the session is not valid.
  ContextualSearchContextController* GetController() const;

  // Returns the ContextualSearchMetricsRecorder reference held by this handle
  // or nullptr if the session is not valid.
  ContextualSearchMetricsRecorder* GetMetricsRecorder() const;

  // Notifies the session handle that the session has started.
  void NotifySessionStarted();

  // Notifies the session handle that the session has been abandoned.
  void NotifySessionAbandoned();

  // Returns the suggest inputs for the current session.
  std::optional<lens::proto::LensOverlaySuggestInputs> GetSuggestInputs() const;

  // Adds a file to the context controller and starts the file upload flow.
  void AddFileContext(std::string file_mime_type,
                      mojo_base::BigBuffer file_bytes,
                      std::optional<lens::ImageEncodingOptions> image_options,
                      AddFileContextCallback callback);

  // Starts the tab context upload flow for the given file token using the
  // tab context stored in the contextual input data.
  void StartTabContextUploadFlow(
      const base::UnguessableToken& file_token,
      std::unique_ptr<lens::ContextualInputData> contextual_input_data,
      std::optional<lens::ImageEncodingOptions> image_options);

  // Removes file from context controller. Returns true if the file was found
  // and deleted.
  bool DeleteFile(const base::UnguessableToken& file_token);

  // Clear all context controller files.
  void ClearFiles();

  // Returns the search url for a new query for opening.
  // TODO(crbug.com/458081018): Create another method for returning a query
  // payload for followup turns.
  GURL CreateSearchUrl(
      std::unique_ptr<contextual_search::ContextualSearchContextController::
                          CreateSearchUrlRequestInfo> search_url_request_info);

 private:
  friend class ContextualSearchService;

  ContextualSearchSessionHandle(base::WeakPtr<ContextualSearchService> service,
                                const SessionId& session_id);

  // The service that vended this handle. This is a weak pointer because a
  // handle may outlive the service.
  const base::WeakPtr<ContextualSearchService> service_;
  const base::UnguessableToken session_id_;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
