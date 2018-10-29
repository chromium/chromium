// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"

#include <iterator>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/blocked_content/list_item_position.h"
#include "chrome/browser/ui/blocked_content/popup_tracker.h"
#include "chrome/browser/ui/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

const size_t kMaximumNumberOfPopups = 25;

struct PopupBlockerTabHelper::BlockedRequest {
  BlockedRequest(NavigateParams&& params,
                 const blink::mojom::WindowFeatures& window_features,
                 PopupBlockType block_type)
      : params(std::move(params)),
        window_features(window_features),
        block_type(block_type) {}

  NavigateParams params;
  blink::mojom::WindowFeatures window_features;
  PopupBlockType block_type;
};

PopupBlockerTabHelper::PopupBlockerTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  SafeBrowsingTriggeredPopupBlocker::MaybeCreate(web_contents);
}

PopupBlockerTabHelper::~PopupBlockerTabHelper() {
}

void PopupBlockerTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PopupBlockerTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PopupBlockerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Clear all page actions, blocked content notifications and browser actions
  // for this tab, unless this is an same-document navigation. Also only
  // consider main frame navigations that successfully committed.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Close blocked popups.
  if (!blocked_popups_.empty()) {
    blocked_popups_.clear();
    PopupNotificationVisibilityChanged(false);
  }
}

void PopupBlockerTabHelper::PopupNotificationVisibilityChanged(
    bool visible) {
  if (!web_contents()->IsBeingDestroyed()) {
    TabSpecificContentSettings::FromWebContents(web_contents())->
        SetPopupsBlocked(visible);
  }
}

void PopupBlockerTabHelper::AddBlockedPopup(
    NavigateParams* params,
    const blink::mojom::WindowFeatures& window_features,
    PopupBlockType block_type) {
  LogAction(Action::kBlocked);
  if (blocked_popups_.size() >= kMaximumNumberOfPopups)
    return;

  int id = next_id_;
  next_id_++;
  blocked_popups_[id] = std::make_unique<BlockedRequest>(
      std::move(*params), window_features, block_type);
  TabSpecificContentSettings::FromWebContents(web_contents())->
      OnContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS);
  for (auto& observer : observers_)
    observer.BlockedPopupAdded(id, blocked_popups_[id]->params.url);
}

void PopupBlockerTabHelper::ShowBlockedPopup(
    int32_t id,
    WindowOpenDisposition disposition) {
  auto it = blocked_popups_.find(id);
  if (it == blocked_popups_.end())
    return;

  ListItemPosition position = GetListItemPositionFromDistance(
      std::distance(blocked_popups_.begin(), it), blocked_popups_.size());

  UMA_HISTOGRAM_ENUMERATION("ContentSettings.Popups.ClickThroughPosition",
                            position, ListItemPosition::kLast);

  BlockedRequest* popup = it->second.get();

  // We set user_gesture to true here, so the new popup gets correctly focused.
  popup->params.user_gesture = true;
  if (disposition != WindowOpenDisposition::CURRENT_TAB)
    popup->params.disposition = disposition;

#if defined(OS_ANDROID)
  TabModelList::HandlePopupNavigation(&popup->params);
#else
  Navigate(&popup->params);
#endif
  if (popup->params.navigated_or_inserted_contents) {
    PopupTracker::CreateForWebContents(
        popup->params.navigated_or_inserted_contents, web_contents());

    if (popup->params.disposition == WindowOpenDisposition::NEW_POPUP) {
      content::RenderFrameHost* host =
          popup->params.navigated_or_inserted_contents->GetMainFrame();
      DCHECK(host);
      chrome::mojom::ChromeRenderFrameAssociatedPtr client;
      host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
      client->SetWindowFeatures(popup->window_features.Clone());
    }
  }

  switch (popup->block_type) {
    case PopupBlockType::kNoGesture:
      LogAction(Action::kClickedThroughNoGesture);
      break;
    case PopupBlockType::kAbusive:
      LogAction(Action::kClickedThroughAbusive);
      break;
  }

  blocked_popups_.erase(id);
  if (blocked_popups_.empty())
    PopupNotificationVisibilityChanged(false);
}

size_t PopupBlockerTabHelper::GetBlockedPopupsCount() const {
  return blocked_popups_.size();
}

PopupBlockerTabHelper::PopupIdMap
    PopupBlockerTabHelper::GetBlockedPopupRequests() {
  PopupIdMap result;
  for (const auto& it : blocked_popups_) {
    result[it.first] = it.second->params.url;
  }
  return result;
}

// static
void PopupBlockerTabHelper::LogAction(Action action) {
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.Popups.BlockerActions", action);
}
