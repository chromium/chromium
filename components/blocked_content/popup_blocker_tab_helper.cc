// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/popup_blocker_tab_helper.h"

#include <iterator>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/blocked_content/list_item_position.h"
#include "components/blocked_content/popup_navigation_delegate.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

namespace blocked_content {
const size_t kMaximumNumberOfPopups = 25;

struct PopupBlockerTabHelper::BlockedRequest {
  BlockedRequest(std::unique_ptr<PopupNavigationDelegate> delegate,
                 const blink::mojom::WindowFeatures& window_features,
                 PopupBlockType block_type)
      : delegate(std::move(delegate)),
        window_features(window_features),
        block_type(block_type) {}

  std::unique_ptr<PopupNavigationDelegate> delegate;
  blink::mojom::WindowFeatures window_features;
  PopupBlockType block_type;
};

PopupBlockerTabHelper::PopupBlockerTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PopupBlockerTabHelper>(*web_contents) {
  blocked_content::SafeBrowsingTriggeredPopupBlocker::MaybeCreate(web_contents);
}

PopupBlockerTabHelper::~PopupBlockerTabHelper() = default;

void PopupBlockerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Clear all page actions, blocked content notifications and browser actions
  // for this tab, unless this is an same-document navigation. Also only
  // consider main frame navigations that successfully committed.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Close blocked popups.
  if (!blocked_popups_.empty()) {
    blocked_popups_.clear();
    HidePopupNotification();

    // With back-forward cache we can restore the page, but |blocked_popups_|
    // are lost here and can't be restored at the moment.
    // Disable bfcache here to avoid potential loss of the page state.
    content::BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetPreviousRenderFrameHostId(),
        back_forward_cache::DisabledReason(
            back_forward_cache::DisabledReasonId::kPopupBlockerTabHelper));
  }
}

void PopupBlockerTabHelper::HidePopupNotification() {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  if (pscs)
    pscs->ClearPopupsBlocked();
}

void PopupBlockerTabHelper::AddBlockedPopup(
    std::unique_ptr<PopupNavigationDelegate> delegate,
    const blink::mojom::WindowFeatures& window_features,
    PopupBlockType block_type) {
  LogAction(Action::kBlocked);
  if (blocked_popups_.size() >= kMaximumNumberOfPopups)
    return;

  int id = next_id_;
  next_id_++;
  blocked_popups_[id] = std::make_unique<BlockedRequest>(
      std::move(delegate), window_features, block_type);

  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  if (content_settings) {
    content_settings->OnContentBlocked(ContentSettingsType::POPUPS);
  }
  auto* raw_delegate = blocked_popups_[id]->delegate.get();

  manager_.NotifyObservers(id, raw_delegate->GetURL());

  raw_delegate->OnPopupBlocked(web_contents(), GetBlockedPopupsCount());
}

void PopupBlockerTabHelper::ShowBlockedPopup(
    int32_t id,
    WindowOpenDisposition disposition) {
  auto it = blocked_popups_.find(id);
  if (it == blocked_popups_.end())
    return;

  BlockedRequest* popup = it->second.get();

  std::optional<WindowOpenDisposition> updated_disposition;
  if (disposition != WindowOpenDisposition::CURRENT_TAB)
    updated_disposition = disposition;

  PopupNavigationDelegate::NavigateResult result =
      popup->delegate->NavigateWithGesture(popup->window_features,
                                           updated_disposition);
  if (result.navigated_or_inserted_contents) {
    auto* tracker = blocked_content::PopupTracker::CreateForWebContents(
        result.navigated_or_inserted_contents, web_contents(),
        result.disposition);
    tracker->set_is_trusted(true);
  }

  switch (popup->block_type) {
    case PopupBlockType::kNotBlocked:
      NOTREACHED_IN_MIGRATION();
      break;
    case PopupBlockType::kNoGesture:
      LogAction(Action::kClickedThroughNoGesture);
      break;
    case PopupBlockType::kAbusive:
      LogAction(Action::kClickedThroughAbusive);
      break;
  }

  blocked_popups_.erase(id);
  if (blocked_popups_.empty())
    HidePopupNotification();
}

void PopupBlockerTabHelper::ShowAllBlockedPopups() {
  PopupIdMap blocked_popups = GetBlockedPopupRequests();
  for (const auto& elem : blocked_popups) {
    ShowBlockedPopup(elem.first, WindowOpenDisposition::CURRENT_TAB);
  }
}

size_t PopupBlockerTabHelper::GetBlockedPopupsCount() const {
  return blocked_popups_.size();
}

PopupBlockerTabHelper::PopupIdMap
PopupBlockerTabHelper::GetBlockedPopupRequests() {
  PopupIdMap result;
  for (const auto& it : blocked_popups_) {
    result[it.first] = it.second->delegate->GetURL();
  }
  return result;
}

// static
void PopupBlockerTabHelper::LogAction(Action action) {
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.Popups.BlockerActions", action);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PopupBlockerTabHelper);

}  // namespace blocked_content
