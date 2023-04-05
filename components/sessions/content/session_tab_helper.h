// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_SESSION_TAB_HELPER_H_
#define COMPONENTS_SESSIONS_CONTENT_SESSION_TAB_HELPER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/sessions_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace sessions {
class SessionTabHelperDelegate;

// This class keeps the extension API's windowID up to date with the current
// window of the tab and observes navigation events.
class SESSIONS_EXPORT SessionTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SessionTabHelper> {
 public:
  using DelegateLookup =
      base::RepeatingCallback<SessionTabHelperDelegate*(content::WebContents*)>;
  using WindowIdChangedCallbackList =
      base::RepeatingCallbackList<void(SessionID id)>;

  SessionTabHelper(const SessionTabHelper&) = delete;
  SessionTabHelper& operator=(const SessionTabHelper&) = delete;

  ~SessionTabHelper() override;

  // Returns the identifier used by session restore for this tab.
  SessionID session_id() const { return session_id_; }

  // Identifier of the window the tab is in.
  void SetWindowID(SessionID id);
  SessionID window_id() const { return window_id_; }

  // If the specified WebContents has a SessionTabHelper (probably because it
  // was used as the contents of a tab), returns a tab id. This value is
  // immutable for a given tab. It will be unique across Chrome within the
  // current session, but may be re-used across sessions. Returns
  // SessionID::InvalidValue() for a null WebContents or if the WebContents has
  // no SessionTabHelper.
  static SessionID IdForTab(const content::WebContents* tab);

  // If the specified WebContents has a SessionTabHelper (probably because it
  // was used as the contents of a tab), and has ever been attached to a Browser
  // window, returns Browser::session_id().id() for that Browser. If the tab is
  // being dragged between Browser windows, returns the old window's id value.
  // If the WebContents has a SessionTabHelper but has never been attached to a
  // Browser window, returns an id value that is different from that of any
  // Browser. Returns SessionID::InvalidValue() for a null WebContents or if the
  // WebContents has no SessionTabHelper.
  static SessionID IdForWindowContainingTab(const content::WebContents* tab);

  base::CallbackListSubscription RegisterForWindowIdChanged(
      WindowIdChangedCallbackList::CallbackType callback);

  // content::WebContentsObserver:
  void UserAgentOverrideSet(
      const blink::UserAgentOverride& ua_override) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void NavigationListPruned(
      const content::PrunedDetails& pruned_details) override;
  void NavigationEntriesDeleted() override;
  void NavigationEntryChanged(
      const content::EntryChangedDetails& change_details) override;

 private:
  friend class content::WebContentsUserData<SessionTabHelper>;
  SessionTabHelper(content::WebContents* contents, DelegateLookup lookup);

  sessions::SessionTabHelperDelegate* GetDelegate();

  WindowIdChangedCallbackList window_id_changed_callbacks_;

  DelegateLookup delegate_lookup_;

  // Unique identifier of the tab for session restore. This id is only unique
  // within the current session, and is not guaranteed to be unique across
  // sessions.
  const SessionID session_id_;

  // Unique identifier of the window the tab is in.
  SessionID window_id_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_SESSION_TAB_HELPER_H_
