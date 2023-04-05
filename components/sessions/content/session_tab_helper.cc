// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/session_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/session_tab_helper_delegate.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_user_agent_override.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"

namespace sessions {

SessionTabHelper::SessionTabHelper(content::WebContents* contents,
                                   DelegateLookup lookup)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<SessionTabHelper>(*contents),
      delegate_lookup_(std::move(lookup)),
      session_id_(SessionID::NewUnique()),
      window_id_(SessionID::InvalidValue()) {}

SessionTabHelper::~SessionTabHelper() = default;

void SessionTabHelper::SetWindowID(SessionID id) {
  window_id_ = id;
  window_id_changed_callbacks_.Notify(id);
}

// static
SessionID SessionTabHelper::IdForTab(const content::WebContents* tab) {
  const SessionTabHelper* session_tab_helper =
      tab ? SessionTabHelper::FromWebContents(tab) : nullptr;
  return session_tab_helper ? session_tab_helper->session_id()
                            : SessionID::InvalidValue();
}

// static
SessionID SessionTabHelper::IdForWindowContainingTab(
    const content::WebContents* tab) {
  const SessionTabHelper* session_tab_helper =
      tab ? SessionTabHelper::FromWebContents(tab) : nullptr;
  return session_tab_helper ? session_tab_helper->window_id()
                            : SessionID::InvalidValue();
}

base::CallbackListSubscription SessionTabHelper::RegisterForWindowIdChanged(
    WindowIdChangedCallbackList::CallbackType callback) {
  return window_id_changed_callbacks_.Add(std::move(callback));
}

void SessionTabHelper::UserAgentOverrideSet(
    const blink::UserAgentOverride& ua_override) {
  SessionTabHelperDelegate* delegate = GetDelegate();
  if (delegate) {
    sessions::SerializedUserAgentOverride serialized_override;
    serialized_override.ua_string_override = ua_override.ua_string_override;
    serialized_override.opaque_ua_metadata_override =
        blink::UserAgentMetadata::Marshal(ua_override.ua_metadata_override);
    delegate->SetTabUserAgentOverride(window_id(), session_id(),
                                      serialized_override);
  }
}

void SessionTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  SessionTabHelperDelegate* delegate = GetDelegate();
  if (!delegate)
    return;

  int current_entry_index =
      web_contents()->GetController().GetCurrentEntryIndex();
  delegate->SetSelectedNavigationIndex(window_id(), session_id(),
                                       current_entry_index);
  const SerializedNavigationEntry navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          current_entry_index,
          web_contents()->GetController().GetEntryAtIndex(current_entry_index));
  delegate->UpdateTabNavigation(window_id(), session_id(), navigation);
}

void SessionTabHelper::NavigationListPruned(
    const content::PrunedDetails& pruned_details) {
  SessionTabHelperDelegate* delegate = GetDelegate();
  if (!delegate)
    return;

  delegate->TabNavigationPathPruned(window_id(), session_id(),
                                    pruned_details.index, pruned_details.count);
}

void SessionTabHelper::NavigationEntriesDeleted() {
  SessionTabHelperDelegate* delegate = GetDelegate();
  if (!delegate)
    return;

  delegate->TabNavigationPathEntriesDeleted(window_id(), session_id());
}

void SessionTabHelper::NavigationEntryChanged(
    const content::EntryChangedDetails& change_details) {
  SessionTabHelperDelegate* delegate = GetDelegate();
  if (!delegate)
    return;

  const SerializedNavigationEntry navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          change_details.index, change_details.changed_entry);
  delegate->UpdateTabNavigation(window_id(), session_id(), navigation);
}

SessionTabHelperDelegate* SessionTabHelper::GetDelegate() {
  return delegate_lookup_ ? delegate_lookup_.Run(web_contents()) : nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SessionTabHelper);

}  // namespace sessions
