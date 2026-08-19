// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"

#include "base/check_op.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "components/tabs/public/tab_interface.h"

FramebustBlockTabHelper::~FramebustBlockTabHelper() = default;

void FramebustBlockTabHelper::AddBlockedUrl(
    const GURL& blocked_url,
    const std::optional<url::Origin>& initiator_origin,
    ClickCallback click_callback) {
  blocked_urls_.push_back(blocked_url);
  initiator_origins_.push_back(initiator_origin);
  callbacks_.push_back(std::move(click_callback));
  DCHECK_EQ(blocked_urls_.size(), callbacks_.size());
  DCHECK_EQ(blocked_urls_.size(), initiator_origins_.size());

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
  if (!callbacks_[index].is_null()) {
    std::move(callbacks_[index]).Run(url, index, total_size);
  }
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/true);
  params.initiator_origin = initiator_origins_[index];
  web_contents()->OpenURL(params, /*navigation_handle_callback=*/{});
}

DEFINE_USER_DATA(FramebustBlockTabHelper);

FramebustBlockTabHelper::FramebustBlockTabHelper(
    tabs::TabInterface& tab,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

// static
FramebustBlockTabHelper* FramebustBlockTabHelper::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

void FramebustBlockTabHelper::PrimaryPageChanged(content::Page& page) {
  blocked_urls_.clear();
  initiator_origins_.clear();
  callbacks_.clear();

  content_settings::UpdateLocationBarUiForWebContents(web_contents());
}
