// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/sessions/core/session_id.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/lens_server_proto/modality_chip_props.pb.h"

class GURL;
class PrefService;

namespace contextual_tasks {
struct ThreadTurn;
}  // namespace contextual_tasks

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
  // Interface to perform platform-specific validation of tabs that were
  // previously uploaded as context.
  class TabValidator {
   public:
    virtual ~TabValidator() = default;

    // Checks if the tab described by `file_info` is still valid.
    //
    // A tab is considered valid if:
    // 1. The tab is still open in the user's browser.
    // 2. The tab is still pointing to the "same" page as when it was uploaded.
    //    "Same page" is determined using platform-specific URL deduplication
    //    logic (e.g., ignoring refs, usernames, passwords, and applying
    //    Doc-specific normalization).
    //
    // Returns:
    // - true: The tab is still open and pointing to the same page.
    // - false: The tab has been closed, or the user navigated away from the
    // page.
    virtual bool IsTabValidAndPointingToUrl(const FileInfo& file_info) = 0;

    // Checks if two URLs are equivalent using the same deduplication logic
    // as `IsTabValidAndPointingToUrl`.
    virtual bool AreUrlsEquivalent(const GURL& url1,
                                   const std::string& title1,
                                   const GURL& url2,
                                   const std::string& title2) = 0;
  };

  ContextualSearchSessionHandle(const ContextualSearchSessionHandle&) = delete;
  ContextualSearchSessionHandle& operator=(
      const ContextualSearchSessionHandle&) = delete;
  ContextualSearchSessionHandle(ContextualSearchSessionHandle&&) = delete;
  ContextualSearchSessionHandle& operator=(ContextualSearchSessionHandle&&) =
      delete;
  virtual ~ContextualSearchSessionHandle();

  // Provides a WeakPtr to this instance. The caller is responsible to only use
  // this on the same sequence that the `ContextualSearchSessionHandle` is
  // destructed on. Inlined to fix linking issues on iOS.
  base::WeakPtr<ContextualSearchSessionHandle> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::UnguessableToken session_id() const { return session_id_; }

  std::optional<bool> smart_tab_sharing_active() const {
    return smart_tab_sharing_active_;
  }
  void set_smart_tab_sharing_active(std::optional<bool> active);

  bool smart_tab_sharing_toggled_since_last_turn() const {
    return smart_tab_sharing_toggled_since_last_turn_;
  }
  void set_smart_tab_sharing_toggled_since_last_turn(bool toggled) {
    smart_tab_sharing_toggled_since_last_turn_ = toggled;
  }

  const std::vector<lens::LensOverlayRequestId>&
  sts_toggled_removed_contexts() const {
    return sts_toggled_removed_contexts_;
  }
  void set_sts_toggled_removed_contexts(
      std::vector<lens::LensOverlayRequestId> contexts) {
    sts_toggled_removed_contexts_ = std::move(contexts);
  }

  std::optional<lens::LensOverlayInvocationSource> invocation_source() const {
    return invocation_source_;
  }

  bool is_contextual_lens_session() const {
    return is_contextual_lens_session_;
  }

  void set_is_contextual_lens_session(bool is_contextual_lens_session) {
    is_contextual_lens_session_ = is_contextual_lens_session;
  }

  // Returns the ContextualSearchContextController reference held by this
  // handle or nullptr if the session is not valid.
  virtual ContextualSearchContextController* GetController() const;

  // Returns the ContextualSearchMetricsRecorder reference held by this handle
  // or nullptr if the session is not valid.
  ContextualSearchMetricsRecorder* GetMetricsRecorder() const;

  // Notifies the session handle that the session has started.
  virtual void NotifySessionStarted();

  // Sets whether or not the session is backgrounded.
  virtual void SetIsBackgrounded(bool backgrounded);

  // Notifies the session handle that the session has been abandoned.
  void NotifySessionAbandoned();

  // Checks the SearchContentSharingSettings policy. Returns true if sharing is
  // allowed, false otherwise. Clients MUST call this method at least once
  // during the lifetime of the session handle before uploading any context, to
  // indicate that the policy has been checked.
  bool CheckSearchContentSharingSettings(const PrefService* prefs);

  // Returns the suggest inputs for the current session.
  virtual std::optional<lens::proto::LensOverlaySuggestInputs>
  GetSuggestInputs() const;

  // Generates a token and adds it to the list of uploaded context tokens. A
  // followup call to 'StartFileContextUploadFlow` or
  // `StartTabContextUploadFlow`, using the returned token, is required to start
  // the upload with the contextual input data.
  virtual base::UnguessableToken CreateContextToken();

  // Adds a file to the context controller and starts the file upload flow. The
  // file token must have been previously returned by `CreateContextToken`.
  virtual void StartFileContextUploadFlow(
      const base::UnguessableToken& file_token,
      std::string file_name,
      std::string file_mime_type,
      mojo_base::BigBuffer file_bytes,
      std::optional<lens::ImageEncodingOptions> image_options);

  // Starts the tab context upload flow for the given file token using the
  // tab context stored in the contextual input data. The file token must have
  // been previously returned by `CreateContextToken`.
  virtual void StartTabContextUploadFlow(
      const base::UnguessableToken& file_token,
      std::unique_ptr<lens::ContextualInputData> contextual_input_data,
      std::optional<lens::ImageEncodingOptions> image_options);

  // Starts the URL context upload flow for the given file token. The file
  // token must have been previously returned by `CreateContextToken`.
  virtual void StartUrlContextUploadFlow(
      const base::UnguessableToken& file_token,
      const std::string& url);

  struct DriveUploadParams {
    std::string drive_id;
    std::optional<std::string> resource_key;
    std::string mime_type;
    std::string file_name;
  };

  // Starts the Drive context upload flow for the given file token. The file
  // token must have been previously returned by `CreateContextToken`.
  virtual void StartDriveContextUploadFlow(
      const base::UnguessableToken& file_token,
      const DriveUploadParams& params);

  // Starts the Modality Chip upload flow for the given file token. The file
  // token must have been previously returned by `CreateContextToken`.
  virtual void StartModalityChipUploadFlow(
      const base::UnguessableToken& file_token,
      std::unique_ptr<lens::ModalityChipProps> modality_chip_props);

  // Removes file from context controller. Returns true if the file was found
  // and deleted.
  bool DeleteFile(const base::UnguessableToken& file_token);

  using DeselectedTabsMap = std::map<SessionID, std::pair<GURL, std::string>>;

  const DeselectedTabsMap& deselected_tabs_urls() const {
    return deselected_tabs_urls_;
  }
  void set_deselected_tabs_urls(DeselectedTabsMap deselected_tabs_urls) {
    deselected_tabs_urls_ = std::move(deselected_tabs_urls);
  }

  // Returns the token for the tab session ID, searching both uploaded and
  // submitted tokens.
  base::UnguessableToken GetTokenForTab(SessionID tab_session_id) const;

  // Checks if a tab is currently deselected. Lazily clears deselection if the
  // tab navigated away from the URL it had when it was deselected.
  bool IsTabDeselected(SessionID tab_session_id,
                       const GURL& current_url,
                       const std::string& current_title) const;

  // Returns true if the two URLs are equivalent using the session's validator.
  bool AreUrlsEquivalent(const GURL& url1,
                         const std::string& title1,
                         const GURL& url2,
                         const std::string& title2) const;

  // Removes a tab from the deselected list (e.g. when it is re-selected).
  void RemoveDeselectedTab(SessionID tab_session_id);

  // Clear all context controller files from this particular instance of the
  // session handle. This does not clear the internal state of the context
  // controller, which may be shared with other session handles.
  // Moves uploaded file tokens that are tabs into `persisted_tabs_` if
  // `query_submitted` is true.
  void ClearFiles(bool query_submitted = false);

  // Returns the search url for a new query for opening. If the request info
  // contains file tokens, only those provided tokens are used. If the request
  // info does not contain file tokens, the uploaded context tokens are moved to
  // the request. In both cases, the files tokens that are used are considered
  // submitted and will be cleared from the context controller.
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
  // Gets a list of file infos for all uploaded context files.
  virtual std::vector<FileInfo> GetUploadedContextFileInfos() const;

  // Returns the list of uploaded but not yet committed context tokens for this
  // particular instance of the session, editable for testing.
  std::vector<base::UnguessableToken>& GetUploadedContextTokensForTesting() {
    return uploaded_context_tokens_;
  }

  // Returns true if the token corresponds to a tab context, for testing.
  bool IsTabTokenForTesting(const base::UnguessableToken& token) const {
    return IsTabToken(token);
  }

  // Returns the active token for a tab, for testing.
  base::UnguessableToken GetActiveTokenForTabForTesting(
      SessionID tab_session_id) const {
    return GetActiveTokenForTab(tab_session_id);
  }

  // Returns the list of submitted context tokens for this particular instance
  // of the session. These are uploaded and submitted, but we have not received
  // confirmation that they are available on the server.
  std::vector<base::UnguessableToken> GetSubmittedContextTokens() const;

  // Returns true if any context tokens were submitted in any query in this
  // session.
  bool has_submitted_context() const { return has_submitted_context_; }

  // Clears the list of submitted context tokens for this particular instance of
  // the session. This is intended to be invoked when the server has responded
  // that it has received the submitted context.
  void ClearSubmittedContextTokens();

  // Sets the submitted context tokens.
  void set_submitted_context_tokens(
      const std::vector<base::UnguessableToken>& tokens);

  using PersistedTabsMap =
      std::map<SessionID,
               std::pair<base::UnguessableToken, lens::LensOverlayRequestId>>;

  // Returns the map of persisted tabs.
  const PersistedTabsMap& persisted_tabs() const { return persisted_tabs_; }

  // Sets the persisted tabs map.
  void set_persisted_tabs(PersistedTabsMap persisted_tabs);

  // Returns the list of submitted FileInfo for this particular instance
  // of the session. These are uploaded and submitted, but we have not received
  // confirmation that they are available on the server.
  virtual std::vector<FileInfo> GetSubmittedContextFileInfos() const;

  // Returns all the tab titles corresponding to the submitted context tokens.
  virtual std::vector<std::string> GetSubmittedContextTabTitles() const;

  // Returns whether the current session_id is part of the uploaded context.
  bool IsTabInContext(SessionID session_id) const;

  // Accessors for the last query and public turns in the contextual session.
  void AddThreadTurn(const contextual_tasks::ThreadTurn& turn);
  const std::vector<contextual_tasks::ThreadTurn>& previous_turns() const {
    return previous_turns_;
  }

 private:
  friend class ContextualSearchService;
  friend class MockContextualSearchSessionHandle;
  FRIEND_TEST_ALL_PREFIXES(
      ContextualSearchSessionHandleTest,
      NotifyQuerySubmittedSessionState_TabAttachmentCount);

  ContextualSearchSessionHandle(
      base::WeakPtr<ContextualSearchService> service,
      const SessionId& session_id,
      std::optional<lens::LensOverlayInvocationSource> invocation_source);

  // Notifies the metrics recorder that a query has been submitted, providing
  // information about the presence of tab and non-tab context.
  void NotifyQuerySubmittedSessionState(const std::vector<FileInfo>& file_infos,
                                        int query_text_length);

  // Returns the active (non-superceded) token for the given tab session ID,
  // or an empty token if not found.
  base::UnguessableToken GetActiveTokenForTab(SessionID tab_session_id) const;

  // Tracks a persisted tab if it is not superceded, deduplicating history.
  void MaybeAddTabToPersistedTabs(const base::UnguessableToken& token);

  // Returns true if the token corresponds to a tab context.
  bool IsTabToken(const base::UnguessableToken& token) const;

  // The list of uploaded context tokens for this particular instance of the
  // session. This list is unique to this instance of the session handle.
  // Note: If kContextManagementInComposebox is enabled, this list can contain
  // tokens that have already been submitted (committed) in a previous query
  // but are still active in the UI.
  // TODO(crbug.com/524332787): Stop using uploaded_context_tokens_ for tab
  // persistence when context management is enabled.
  std::vector<base::UnguessableToken> uploaded_context_tokens_;

  // The list of uploaded and submitted, but not yet committed context tokens
  // for this particular instance of the session. This list is unique to this
  // instance of the session handle, meaning that it is unique per instance of
  // the contextual tasks ui.
  std::vector<base::UnguessableToken> submitted_context_tokens_;

  // Whether any context tokens were submitted in a query in this session.
  bool has_submitted_context_ = false;

  // Map of tab session IDs to their latest submitted token and request ID.
  // Tracks active tabs in the session to detect their deletion or removal.
  std::map<SessionID,
           std::pair<base::UnguessableToken, lens::LensOverlayRequestId>>
      persisted_tabs_;

  // Tracks tabs explicitly deselected by the user. Map key is the SessionID,
  // and value is the GURL of the tab at the time of deselection.
  mutable DeselectedTabsMap deselected_tabs_urls_;

  // Whether the SearchContentSharingSettings policy has been checked.
  bool policy_checked_ = false;

  // Returns the tab validator if the service is still alive.
  TabValidator* GetTabValidator() const;

  // The service that vended this handle. This is a weak pointer because a
  // handle may outlive the service.
  const base::WeakPtr<ContextualSearchService> service_;
  const base::UnguessableToken session_id_;

  // The invocation source to send with generated search URLs or query payloads.
  const std::optional<lens::LensOverlayInvocationSource> invocation_source_;

  // Whether this session was initiated by a contextual Lens query. This could
  // apply to entrypoints like contextual suggestions in the Omnibox or the
  // contextual searchbox within the Lens overlay.
  bool is_contextual_lens_session_ = false;

  // The list of previous turns in the contextual session, from oldest to
  // newest.
  std::vector<contextual_tasks::ThreadTurn> previous_turns_;

  // Whether smart tab sharing is active for this session.
  std::optional<bool> smart_tab_sharing_active_;

  // Whether smart tab sharing was toggled since the last query submission
  // (either smart tab sharing to manual or manual to smart tab sharing) and we
  // need to clear the context on next query submission.
  // This is reset after the next query submission.
  bool smart_tab_sharing_toggled_since_last_turn_ = false;

  // Request IDs of submitted and uploaded contexts collected when Smart Tab
  // Sharing was toggled, to be sent to AIM via `removed_contexts` on the next
  // query submission turn.
  std::vector<lens::LensOverlayRequestId> sts_toggled_removed_contexts_;

  // This needs to be the last member to ensure all outstanding WeakPtrs are
  // invalidated before the rest of the members.
  base::WeakPtrFactory<ContextualSearchSessionHandle> weak_ptr_factory_{this};
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
