// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_activation_tracker.h"

#include "base/check.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"

namespace send_tab_to_self {

namespace {
const char kSendTabToSelfEntryGUIDKey[] = "send_tab_to_self.entry_guid";
}  // namespace

SendTabToSelfActivationTracker::SendTabToSelfActivationTracker(
    content::WebContents* web_contents,
    const std::string& guid)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SendTabToSelfActivationTracker>(
          *web_contents),
      entry_guid_(guid) {
  // Verify that the tab is not already visible during creation.
  CHECK_NE(web_contents->GetVisibility(), content::Visibility::VISIBLE);
  PersistGUID();
}

SendTabToSelfActivationTracker::~SendTabToSelfActivationTracker() = default;

void SendTabToSelfActivationTracker::WebContentsDestroyed() {
  if (!metric_recorded_ && !GetWebContents().WasDiscarded() &&
      !browser_shutdown::HasShutdownStarted()) {
    send_tab_to_self::MarkEntryMatchingGuidActivated(
        Profile::FromBrowserContext(GetWebContents().GetBrowserContext()),
        entry_guid_,
        ShareActivatedEntryPoint::kTabOrBrowserClosedWithoutActivation);
  }
}

// static
void SendTabToSelfActivationTracker::SetEntryOpenedViaToast(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  if (auto* tracker = FromWebContents(web_contents)) {
    tracker->opened_via_toast_ = true;
  }
}

// static
void SendTabToSelfActivationTracker::RestoreFromExtraData(
    content::WebContents* web_contents,
    const std::map<std::string, std::string>& extra_data) {
  auto it = extra_data.find(kSendTabToSelfEntryGUIDKey);
  if (it == extra_data.end()) {
    return;
  }
  const std::string& guid = it->second;
  if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    // If restored in the foreground, no tracker exists, so the entry is
    // marked activated directly. It is assumed to be activated via the tab
    // strip.
    send_tab_to_self::MarkEntryMatchingGuidActivated(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()), guid,
        ShareActivatedEntryPoint::kTabStrip);
    return;
  }

  // Otherwise, create/restore the tracker in the background.
  CreateForWebContents(web_contents, guid);
}

void SendTabToSelfActivationTracker::PersistGUID() {
  // Only the GUID needs to be persisted. `opened_via_toast_` does not
  // need to be persisted because toast notifications do not survive browser
  // restarts. Any activation after a restart is inherently a tab strip
  // activation.
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile);
  if (session_service) {
    SessionID window_id =
        sessions::SessionTabHelper::IdForWindowContainingTab(&GetWebContents());
    SessionID tab_id = sessions::SessionTabHelper::IdForTab(&GetWebContents());
    if (window_id.is_valid() && tab_id.is_valid()) {
      session_service->AddTabExtraData(window_id, tab_id,
                                       kSendTabToSelfEntryGUIDKey, entry_guid_);
    }
  }
}

void SendTabToSelfActivationTracker::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    // Only record when the tab becomes visible to the user.
    return;
  }
  metric_recorded_ = true;

  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  send_tab_to_self::MarkEntryMatchingGuidActivated(
      profile, entry_guid_,
      opened_via_toast_ ? ShareActivatedEntryPoint::kDesktopToast
                        : ShareActivatedEntryPoint::kTabStrip);

  // Self-destruct now that the metric has been recorded.
  // This deletes `this`, so the method must return immediately.
  GetWebContents().RemoveUserData(UserDataKey());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SendTabToSelfActivationTracker);

}  // namespace send_tab_to_self
