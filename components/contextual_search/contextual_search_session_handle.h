// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
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
#include "url/gurl.h"

class PrefService;

namespace contextual_tasks {
struct ThreadTurn;
}  // namespace contextual_tasks

namespace lens {
class ClientToAimMessage;
struct ContextualInputData;
class ModalityChipProps;
enum class MimeType;
namespace proto {
class LensOverlaySuggestInputs;
}  // namespace proto
}  // namespace lens

namespace contextual_search {
using SessionId = base::UnguessableToken;
class ContextualSearchService;
using AddFileContextCallback =
    base::OnceCallback<void(const ::base::UnguessableToken&)>;

// Information about a tab attached to or restored in the contextual search
// session.
struct TabInfo {
  bool operator==(const TabInfo& other) const;

  // Stable client-side identifier, taken from `FileInfo::file_token`.
  // Empty for restored tabs (`restored_from_aim == true`), as they
  // were not uploaded by this session.
  base::UnguessableToken context_token;

  // Browser tab SessionID (`FileInfo::tab_session_id->id()`), if known.
  std::optional<int32_t> tab_id;

  // Server-side Lens request ID for this tab upload, if assigned.
  std::optional<lens::LensOverlayRequestId> request_id;

  GURL url;
  std::string title;

  // True if the tab is explicitly uploaded (no delay) or persisted in the
  // session; false for tabs not uploaded: delayed/implicit uploads (e.g., smart
  // tab, etc.), submitted but not confirmed uploaded tabs, and restored tabs.
  bool uploaded = false;

  // True if the tab was restored from durable task context (e.g. an AIM
  // thread's history) rather than attached in this session.
  bool restored_from_aim = false;

  // True once the tab has been sent to the server in a submitted query. Tabs
  // are remembered after submitting so they can be removed if the user
  // manually removes them.
  bool submitted = false;

  // True if the tab was added via `AddPendingDelayedTab()` and has not been
  // uploaded yet. `context_token` is then not an uploaded context token. Reset
  // once the tab is uploaded via `StartTabContextUploadFlow()`.
  bool pending_delayed_upload = false;
};

// The set of tabs attached or restored as context for a session, as a snapshot.
//
// `attached` and `restored` are vectors rather than maps keyed by tab ID:
// - Order matters. Entries stay in the order tabs were attached (or the order
//   the server returned them), which the UI relies on to render chips and
//   favicons.
// - Lookups by tab ID are O(n), but n is small (a few dozen tabs at most), so
//   a linear scan is cheap.
// - `std::map` is not contiguous: every entry is a separate heap allocation,
//   so for small collections it is usually slower to search and iterate than
//   a vector. See base/containers/README.md.
struct TabContextState {
  // Map of tab SessionID to the tab's URL and title.
  using DeselectedTabsMap = std::map<SessionID, std::pair<GURL, std::string>>;

  bool operator==(const TabContextState& other) const;

  // Returns whether `tab_id` is present in either `attached` or `restored`.
  bool ContainsTab(SessionID tab_id) const;

  // Tabs attached in this session, in the order they were attached. Kept in
  // sync by `StartTabContextUploadFlow()`, `AttachTabPendingUpload()`,
  // `AddPendingDelayedTab()`, `DeleteFile()` and
  // `RemoveUploadedContextToken()`. If `submitted=false` in `ClearFiles()`,
  // all attached tabs should be cleared; otherwise, no change. Delayed tabs
  // are attached with `uploaded` false before their upload starts, and are
  // changed to true once uploaded. Does not include restored tabs, only
  // tabs manually attached in the current session. Frontend or page will
  // combine them/reverse order as needed based on current product
  // requirements. At most one entry per tab ID.
  std::vector<TabInfo> attached;

  // Tabs carried from past history in this thread from the server, in the
  // order the server returned them. May overlap with `attached` if a restored
  // tab is also re-attached in this session; frontend surfaces that render
  // both should deduplicate on URL.
  std::vector<TabInfo> restored;

  // Tabs explicitly deselected by the user in this session, with their URL and
  // title at the time of deselection, so deselection can be lazily cleared if
  // the tab navigates elsewhere.
  mutable DeselectedTabsMap deselected;
};

// RAII handle for managing the lifetime of a ComposeboxQueryController.
class ContextualSearchSessionHandle {
 public:
  using TabContextSubscriber =
      base::RepeatingCallback<void(const TabContextState&)>;
  // Interface to perform platform-specific validation of tabs that were
  // previously uploaded as context.
  class TabValidator {
   public:
    virtual ~TabValidator() = default;

    // Checks if the tab with `tab_session_id` is still open in the user's
    // browser.
    virtual bool IsTabOpen(SessionID tab_session_id) const = 0;

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
  void set_smart_tab_sharing_active(std::optional<bool> active) {
    smart_tab_sharing_active_ = active;
  }

  // Invoked when the user explicitly toggles Smart Tab Sharing in the UI.
  // Clears active/persisted context tokens and records removed context IDs so
  // that tab strip underlines clear immediately and the server drops context.
  void OnSmartTabSharingToggled(bool active);

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

  void set_invocation_source(
      std::optional<lens::LensOverlayInvocationSource> invocation_source) {
    invocation_source_ = invocation_source;
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
  virtual ContextualSearchMetricsRecorder* GetMetricsRecorder() const;

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

  // Returns the search session ID from the underlying controller, or empty
  // string if not available.
  virtual std::string search_session_id() const;

  // Constructs the visual search interaction data for the given file token, if
  // available.
  virtual std::optional<lens::LensOverlayVisualSearchInteractionData>
  GetVisualSearchInteractionData(const base::UnguessableToken& file_token,
                                 const std::optional<std::string>& query_text);

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
  // been previously returned by `CreateContextToken`. If
  // `contextual_input_data` is null, the upload flow is aborted and
  // `file_token` is removed from uploaded context tokens.
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

  // Removes the token from the list of uploaded context tokens. Returns true if
  // the token was found and removed.
  bool RemoveUploadedContextToken(const base::UnguessableToken& file_token);

  using DeselectedTabsMap = TabContextState::DeselectedTabsMap;

  const DeselectedTabsMap& deselected_tabs_urls() const {
    return tab_context_.deselected;
  }
  void set_deselected_tabs_urls(DeselectedTabsMap deselected_tabs_urls) {
    tab_context_.deselected = std::move(deselected_tabs_urls);
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

  // Records `tab_session_id` as explicitly deselected at `url` and `title`.
  void AddDeselectedTab(SessionID tab_session_id,
                        const GURL& url,
                        const std::string& title);

  // Removes a tab from the deselected list (e.g. when it is re-selected).
  void RemoveDeselectedTab(SessionID tab_session_id);

  // Clear all context controller files from this particular instance of the
  // session handle. This does not clear the internal state of the context
  // controller, which may be shared with other session handles.
  // Marks uploaded tabs in `tab_context_.attached` as 'submitted' if
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

  // Returns the client to aim message for a new query for posting. Equivalent
  // to calling `TakeRemovedContexts()` and `MarkQuerySubmitted()` and then
  // building the request from the controller.
  lens::ClientToAimMessage CreateClientToAimRequest(
      std::unique_ptr<contextual_search::ContextualSearchContextController::
                          CreateClientToAimRequestInfo>
          create_client_to_aim_request_info);

  // Returns the request IDs of contexts removed since the last query that the
  // server has not yet been told about: contexts dropped by a Smart Tab Sharing
  // toggle, and persisted attached tabs that were deselected, closed, or
  // navigated away from their uploaded page. Clears the returned contexts from
  // this handle, so each removal is only reported once. Callers that submit
  // queries outside of `CreateClientToAimRequest()` must call this on every
  // submission and forward the result to the server as removed contexts.
  std::vector<lens::LensOverlayRequestId> TakeRemovedContexts();

  // Records that a query was submitted with `file_tokens` plus any uploaded
  // context tokens not already in `file_tokens`. Moves submitted tabs in
  // `tab_context_.attached` as 'submitted', clears `uploaded_context_tokens_`,
  // appends to the submitted context tokens, and records query metrics when
  // `query_text_length` is provided. Returns the full list of submitted
  // tokens in upload order. Callers that submit queries outside of
  // `CreateClientToAimRequest()` must call this on every submission.
  std::vector<base::UnguessableToken> MarkQuerySubmitted(
      std::vector<base::UnguessableToken> file_tokens,
      std::optional<size_t> query_text_length);

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

  // Returns `submitted_context_tokens_` combined with any non-deselected
  // persisted submitted tab tokens in `tab_context_.attached` (deduplicated).
  std::vector<base::UnguessableToken> GetActiveSubmittedContextTokens() const;

  // Returns true if any context tokens were submitted in any query in this
  // session.
  bool has_submitted_context() const { return has_submitted_context_; }

  // The multi-login account index for this session, received from the
  // AIM/Search handshake response (only populated post-rearchitecture when
  // `kContextualTasksRearchitecture` is enabled; 0 otherwise).
  size_t auth_user_index() const { return auth_user_index_; }
  void set_auth_user_index(size_t auth_user_index);

  // The current URL of the WebContents associated with the `InputStateModel`
  // for this session. For the NTP composebox, this is `chrome://new-tab-page`;
  // for the Omnibox, this is the active tab's URL; for the Contextual Tasks
  // side panel, this is the AIM page URL (`www.google.com/search`), either from
  // the embedded `<webview>` frame (pre-rearchitecture) or the page the
  // extension is embedded into (post-rearchitecture).
  const GURL& current_url() const { return current_url_; }
  void set_current_url(const GURL& url) { current_url_ = url; }

  // Clears the list of submitted context tokens for this particular instance of
  // the session. This is intended to be invoked when the server has responded
  // that it has received the submitted context.
  void ClearSubmittedContextTokens();

  // Sets the submitted context tokens.
  void SetSubmittedContextTokens(
      const std::vector<base::UnguessableToken>& tokens);

  using PersistedTabsMap =
      std::map<SessionID,
               std::pair<base::UnguessableToken, lens::LensOverlayRequestId>>;

  // Returns the tabs submitted in previous turns of this session, i.e. the
  // entries of `tab_context_.attached` with `submitted` set.
  PersistedTabsMap GetPersistedTabs() const;

  // Delete all tabs that have persisted across submissions.
  void ClearAllPersistedTabs();

  // Replaces the tabs submitted in previous turns of this session. Used for
  // session handoff.
  void SetPersistedTabs(PersistedTabsMap new_persisted_tabs);

  // Returns the list of submitted FileInfo for this particular instance
  // of the session. These are uploaded and submitted, but we have not received
  // confirmation that they are available on the server.
  virtual std::vector<FileInfo> GetSubmittedContextFileInfos() const;

  // Returns all the tab titles corresponding to the submitted context tokens.
  virtual std::vector<std::string> GetSubmittedContextTabTitles() const;

  // Returns whether the current session_id is part of the uploaded context.
  bool IsTabInContext(SessionID session_id) const;

  // Returns whether any submitted or uploaded file in this session represents
  // PDF context, excluding implicit initial Lens overlay query uploads.
  bool HasPdfContext() const;

  // Accessors for the last query and public turns in the contextual session.
  void AddThreadTurn(const contextual_tasks::ThreadTurn& turn);
  const std::vector<contextual_tasks::ThreadTurn>& previous_turns() const {
    return previous_turns_;
  }

  // Adds a subscriber that is notified whenever the set of attached or
  // restored tabs changes. Call `GetTabContextState()` for the value at
  // subscribe time.
  //
  // Lifetime: `base::CallbackListSubscription` will automatically unregister
  // and cancel the callback if the subscriber is destroyed.
  [[nodiscard]] base::CallbackListSubscription SubscribeTabContext(
      TabContextSubscriber callback);

  // Returns the current snapshot of attached and restored tabs.
  const TabContextState& GetTabContextState() const;

  // Returns the set of valid `SessionID`s across `attached` and `restored`
  // tabs that are not currently marked as deselected.
  base::flat_set<SessionID> GetNonDeselectedTabSessionIds() const;

  // Replace the set of tabs restored from durable task context. Unlike
  // attached tabs, these come from the AIM thread's history in this thread from
  // the server. Each restored tab has `restored_from_aim` set to true
  // and `uploaded` set to false. Notifies tab context subscribers if the set
  // changed.
  void SetRestoredTabs(std::vector<TabInfo> tabs);

  // Attaches a tab whose context, identified by `file_token`, is uploaded
  // later, e.g. while its page context is being captured, or when the upload
  // is delayed until query submission. Records the tab's URL, title and
  // `tab_id`, its SessionID (`SessionID::id()`). It is attached with
  // `uploaded` set to false and is updated in place when
  // `StartTabContextUploadFlow()` is called for the same tab. No-op if
  // `file_token` is not an uploaded context token. Along with
  // `AddPendingDelayedTab()`, this is one of the only functions that must be
  // called outside of usual session handle workflow in order to update the
  // `TabContextState`. Use it when a context token already exists for the tab;
  // otherwise use `AddPendingDelayedTab()`.
  void AttachTabPendingUpload(const base::UnguessableToken& file_token,
                              int32_t tab_id,
                              const GURL& url,
                              const std::string& title);

  // Records a delayed tab that has no context token yet, because its context
  // is captured and uploaded at submit time (e.g. via QueryContextualizer).
  // The session handle creates a token for the tab, which identifies it until
  // then (e.g. for its chip) but is not an uploaded context token, and sets
  // `TabInfo::pending_delayed_upload`. The entry is updated in place when
  // `StartTabContextUploadFlow()` is called for the same tab, and is removed by
  // `DeleteFile()` or `RemoveUploadedContextToken()` with the returned token,
  // or by `ClearFiles(/*query_submitted=*/false)`. Clears any deselection of
  // the tab. Returns the token.
  base::UnguessableToken AddPendingDelayedTab(int32_t tab_id,
                                              const GURL& url,
                                              const std::string& title);

  // Returns the SessionIDs of attached tabs with
  // `TabInfo::pending_delayed_upload`, in attach order.
  std::vector<SessionID> GetPendingDelayedTabIds() const;

  // Deletes the tab that `token` refers to, e.g. when the user removes its
  // chip. Unlike `DeleteFile()`, which deletes exactly `token` and keeps the
  // tab attached if `token` is no longer its current token, this also deletes
  // the tab's current token. The chip's token may be stale: superseded by a
  // re-upload of the tab (e.g. on submit), or returned by
  // `AddPendingDelayedTab()` before the tab was uploaded. Each token is
  // deleted via `DeleteFile()`, or else `RemoveUploadedContextToken()`, so
  // non-tab tokens are just deleted. Returns the `TabInfo::tab_id` of the
  // attached tab for `token`, if any.
  std::optional<int32_t> DeleteTabContext(const base::UnguessableToken& token);

  // Deletes all uploaded, submitted, attached, and restored tab context entries
  // matching `session_id` (e.g. when unselecting a tab by ID). When tab
  // deselection is enabled and the tab was already submitted or restored,
  // records `session_id` as deselected at `current_url` and `current_title`
  // (falling back to the stored tab URL/title if `current_url` is invalid) and
  // queues any submitted `FileInfo` for server-side removal reporting in
  // `TakeRemovedContexts()`. Returns the removed tokens if `session_id` was in
  // `attached` or `restored`, or `std::nullopt` otherwise.
  std::optional<std::vector<base::UnguessableToken>> DeleteTabContext(
      SessionID session_id,
      const GURL& current_url,
      const std::string& current_title);

  // Removes attached, uploaded, submitted, or restored context for
  // `tab_session_id` when the tab is closed (`current_url` is `std::nullopt`)
  // or navigates to a non-equivalent URL (`current_url` is present). Submitted
  // tabs are retained in `displaced_submitted_tabs_` so `TakeRemovedContexts()`
  // reports their `request_id` on the next query submission. Returns the
  // context tokens that were removed, if any.
  std::vector<base::UnguessableToken> OnTabClosedOrNavigated(
      SessionID tab_session_id,
      std::optional<GURL> current_url,
      std::optional<std::string> current_title);

 private:
  friend class ContextualSearchService;
  friend class MockContextualSearchSessionHandle;

  FRIEND_TEST_ALL_PREFIXES(
      ContextualSearchSessionHandleTest,
      NotifyQuerySubmittedSessionState_TabAttachmentCount);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchSessionHandleTest,
                           AddAttachedTabAppendsInOrderAndUpdatesInPlace);

  ContextualSearchSessionHandle(
      base::WeakPtr<ContextualSearchService> service,
      const SessionId& session_id,
      std::optional<lens::LensOverlayInvocationSource> invocation_source);

  // Notifies tab context subscribers of the current `tab_context_`.
  void NotifyTabContextSubscribers();

  // Adds `tab` to the attached tabs. `tab.tab_id` must be set. If a tab with
  // the same ID is already attached, it is updated in place so it keeps its
  // position; otherwise it is appended. `restored_from_aim` is forced to
  // false. Notifies tab context subscribers if the attached tabs changed.
  // Called by normal session handle workflow via `StartTabContextUploadFlow()`
  // and `AttachTabPendingUpload()`.
  void AddAttachedTab(TabInfo tab);

  // Removes attached tabs whose `context_token` is `file_token`, moving any
  // submitted entries to `displaced_submitted_tabs_`. Notifies tab context
  // subscribers if any tabs were removed.
  void RemoveAttachedTabForToken(const base::UnguessableToken& file_token);

  // Notifies tab context subscribers if tab context has been updated
  // (`tab_context_dirty_` is set to `true`). Clears the dirty flag.
  void MaybeNotifyTabContextSubscribers();

  // Notifies the metrics recorder that a query has been submitted, providing
  // information about the presence of tab and non-tab context.
  void NotifyQuerySubmittedSessionState(const std::vector<FileInfo>& file_infos,
                                        int query_text_length);

  // Returns the active (non-superceded) token for the given tab session ID,
  // or an empty token if not found.
  base::UnguessableToken GetActiveTokenForTab(SessionID tab_session_id) const;

  // Marks the tab for `token` in `tab_context_.attached` as `submitted`
  // if it is not already done so. O(n) search to find duplicates is okay
  // since tab number is small.
  void MarkTabAckByServer(const base::UnguessableToken& token);

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

  // Whether an explicit (non-implicit-initial-query) PDF was submitted in a
  // query in this session. Retained across `ClearSubmittedContextTokens()` so
  // `HasPdfContext()` remains true after AIM commits the context library.
  bool has_submitted_pdf_context_ = false;

  // Tab of each token returned by `AddPendingDelayedTab()`, which is replaced
  // in the attached tab once the tab is uploaded. Lets the token's chip delete
  // the tab after that. Erased by `DeleteFile()` with the token.
  base::flat_map<base::UnguessableToken, SessionID> delayed_tab_token_ids_;

  // Current snapshot of attached, restored, and deselected tabs. Entries in
  // `attached` with `submitted` are persistent tabs that are still tracked
  // for the purpose of deleting them from server if manually removed by user.
  TabContextState tab_context_;

  // Submitted tabs that were removed or replaced in `tab_context_.attached`
  // and are awaiting server-side removal inspection in `TakeRemovedContexts()`.
  // TODO(crbug.com/563055721): Consolidate persisted and submitted tab
  // tracking out of `tab_context_.attached` into a unified submitted
  // list and remove `displaced_submitted_tabs_`.
  std::vector<TabInfo> displaced_submitted_tabs_;

  // Subscribers to changes in the tab context state.
  base::RepeatingCallbackList<void(const TabContextState&)>
      tab_context_subscribers_;

  // This flag is set when `tab_context_.attached` and/or
  // `tab_context_.restored` change. It is cleared by
  // `MaybeNotifyTabContextSubscribers()`.
  bool tab_context_dirty_ = false;

  // Whether the SearchContentSharingSettings policy has been checked.
  bool policy_checked_ = false;

  // Returns the tab validator if the service is still alive.
  TabValidator* GetTabValidator() const;

  // The service that vended this handle. This is a weak pointer because a
  // handle may outlive the service.
  const base::WeakPtr<ContextualSearchService> service_;
  const base::UnguessableToken session_id_;

  // The invocation source to send with generated search URLs or query payloads.
  std::optional<lens::LensOverlayInvocationSource> invocation_source_;

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

  // The multi-login account index for this session.
  size_t auth_user_index_ = 0;

  // The current URL of the composebox/AIM WebContents for this session.
  GURL current_url_;

  // This needs to be the last member to ensure all outstanding WeakPtrs are
  // invalidated before the rest of the members.
  base::WeakPtrFactory<ContextualSearchSessionHandle> weak_ptr_factory_{this};
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
