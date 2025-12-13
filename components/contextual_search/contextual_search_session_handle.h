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
using AddTabContextCallback =
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
  virtual ~ContextualSearchSessionHandle();

  // Provides a WeakPtr to this instance. The caller is responsible to only use
  // this on the same sequence that the `ContextualSearchSessionHandle` is
  // destructed on.
  base::WeakPtr<ContextualSearchSessionHandle> AsWeakPtr();

  base::UnguessableToken session_id() const { return session_id_; }

  // Returns the ContextualSearchContextController reference held by this
  // handle or nullptr if the session is not valid.
  ContextualSearchContextController* GetController() const;

  // Returns the ContextualSearchMetricsRecorder reference held by this handle
  // or nullptr if the session is not valid.
  ContextualSearchMetricsRecorder* GetMetricsRecorder() const;

  // Notifies the session handle that the session has started.
  virtual void NotifySessionStarted();

  // Notifies the session handle that the session has been abandoned.
  void NotifySessionAbandoned();

  // Returns the suggest inputs for the current session.
  std::optional<lens::proto::LensOverlaySuggestInputs> GetSuggestInputs() const;

  // Adds a file to the context controller and starts the file upload flow.
  void AddFileContext(std::string file_mime_type,
                      mojo_base::BigBuffer file_bytes,
                      std::optional<lens::ImageEncodingOptions> image_options,
                      AddFileContextCallback callback);

  // Adds a tab context to the context controller, generating a token and adding
  // it to the list of uploaded context tokens. A followup call to
  // `StartTabContextUploadFlow`, using the token returned in the callback,
  // is required to start the upload with the
  // contextual input data.
  // TODO(crbug.com/461869881): Pass more metadata than just the tab id for
  //  being able to return the list of attached tabs.
  void AddTabContext(int32_t tab_id, AddTabContextCallback callback);

  // Starts the tab context upload flow for the given file token using the
  // tab context stored in the contextual input data.
  virtual void StartTabContextUploadFlow(
      const base::UnguessableToken& file_token,
      std::unique_ptr<lens::ContextualInputData> contextual_input_data,
      std::optional<lens::ImageEncodingOptions> image_options);

  // Removes file from context controller. Returns true if the file was found
  // and deleted.
  bool DeleteFile(const base::UnguessableToken& file_token);

  // Clear all context controller files from this particular instance of the
  // session handle. This does not clear the internal state of the context
  // controller, which may be shared with other session handles.
  void ClearFiles();

  // Returns the search url for a new query for opening.
  virtual void CreateSearchUrl(
      std::unique_ptr<contextual_search::ContextualSearchContextController::
                          CreateSearchUrlRequestInfo> search_url_request_info,
      base::OnceCallback<void(GURL)> callback);

  // Returns the client to aim message for a new query for posting.
  lens::ClientToAimMessage CreateClientToAimRequest(
      std::unique_ptr<contextual_search::ContextualSearchContextController::
                          CreateClientToAimRequestInfo>
          create_client_to_aim_request_info);

  // Returns the list of uploaded but not yet committed context tokens for this
  // particular instance of the session.
  std::vector<base::UnguessableToken> GetUploadedContextTokens() const;

  // Returns the list of uploaded but not yet committed FileInfo for this
  // particular instance of the session.
  std::vector<FileInfo> GetUploadedContextFileInfos() const;

  // Returns the list of uploaded but not yet committed context tokens for this
  // particular instance of the session, editable for testing.
  std::vector<base::UnguessableToken>& GetUploadedContextTokensForTesting() {
    return uploaded_context_tokens_;
  }

  // Returns the list of submitted context tokens for this particular instance
  // of the session. These are uploaded and submitted, but we have not received
  // confirmation that they are available on the server.
  std::vector<base::UnguessableToken> GetSubmittedContextTokens() const;

  // Clears the list of submitted context tokens for this particular instance of
  // the session. This is intended to be invoked when the server has responded
  // that it has received the submitted context.
  void ClearSubmittedContextTokens();

  // Returns the list of submitted FileInfo for this particular instance
  // of the session. These are uploaded and submitted, but we have not received
  // confirmation that they are available on the server.
  std::vector<FileInfo> GetSubmittedContextFileInfos() const;

 private:
  friend class ContextualSearchService;
  friend class MockContextualSearchSessionHandle;

  ContextualSearchSessionHandle(base::WeakPtr<ContextualSearchService> service,
                                const SessionId& session_id);

  // The list of uploaded but not yet committed context tokens for this
  // particular instance of the session. This list is unique to this instance of
  // the session handle, meaning that it is unique per instance of the
  // contextual tasks ui.
  std::vector<base::UnguessableToken> uploaded_context_tokens_;

  // The list of uploaded and submitted, but not yet committed context tokens
  // for this particular instance of the session. This list is unique to this
  // instance of the session handle, meaning that it is unique per instance of
  // the contextual tasks ui.
  std::vector<base::UnguessableToken> submitted_context_tokens_;

  // The service that vended this handle. This is a weak pointer because a
  // handle may outlive the service.
  const base::WeakPtr<ContextualSearchService> service_;
  const base::UnguessableToken session_id_;

  // This needs to be the last member to ensure all outstanding WeakPtrs are
  // invalidated before the rest of the members.
  base::WeakPtrFactory<ContextualSearchSessionHandle> weak_ptr_factory_{this};
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
