// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"

FramebustBlockTabHelper::~FramebustBlockTabHelper() = default;

void FramebustBlockTabHelper::AddBlockedUrl(const GURL& blocked_url,
                                            ClickCallback click_callback) {
  blocked_urls_.push_back(blocked_url);
  callbacks_.push_back(std::move(click_callback));
  DCHECK_EQ(blocked_urls_.size(), callbacks_.size());

  for (Observer& observer : observers_) {
    observer.OnBlockedUrlAdded(blocked_url);
  }
}

bool FramebustBlockTabHelper::HasBlockedUrls() const {
  return !blocked_urls_.empty();
}

void FramebustBlockTabHelper::OnBlockedUrlClicked(size_t index) {
  size_t total_size = blocked_urls_.size();
  DCHECK_LT(index, total_size);
  const GURL& url = blocked_urls_[index];
  if (!callbacks_[index].is_null())
    std::move(callbacks_[index]).Run(url, index, total_size);
  web_contents()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void FramebustBlockTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FramebustBlockTabHelper::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

FramebustBlockTabHelper::FramebustBlockTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void FramebustBlockTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  blocked_urls_.clear();
  callbacks_.clear();
  animation_has_run_ = false;

  // TODO(csharrison): It is a bit ugly that this tab helper has to notify this
  // change directly. Consider improving this by integrating framebust
  // information with the TabSpecificContentSetting class. This may be
  // challenging, since popups and framebusts are controlled by the same content
  // setting.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}
