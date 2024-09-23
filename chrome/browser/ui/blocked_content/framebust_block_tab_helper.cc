// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"

#include "base/check_op.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"

FramebustBlockTabHelper::~FramebustBlockTabHelper() = default;

void FramebustBlockTabHelper::AddBlockedUrl(const GURL& blocked_url,
                                            ClickCallback click_callback) {
  blocked_urls_.push_back(blocked_url);
  callbacks_.push_back(std::move(click_callback));
  DCHECK_EQ(blocked_urls_.size(), callbacks_.size());

  manager_.NotifyObservers(0 /* id */, blocked_url);
  content_settings::UpdateLocationBarUiForWebContents(web_contents());
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
  web_contents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

FramebustBlockTabHelper::FramebustBlockTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<FramebustBlockTabHelper>(*web_contents) {}

void FramebustBlockTabHelper::PrimaryPageChanged(content::Page& page) {
  blocked_urls_.clear();
  callbacks_.clear();

  content_settings::UpdateLocationBarUiForWebContents(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FramebustBlockTabHelper);
