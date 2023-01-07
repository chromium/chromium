// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_FRAMEBUST_BLOCK_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_FRAMEBUST_BLOCK_TAB_HELPER_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/blocked_content/url_list_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

// A tab helper that keeps track of blocked Framebusts that happened on each
// page. Only used for the desktop version of the blocked Framebust UI.
class FramebustBlockTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<FramebustBlockTabHelper> {
 public:
  using ClickCallback = base::OnceCallback<
      void(const GURL&, size_t /* index */, size_t /* total_size */)>;

  FramebustBlockTabHelper(const FramebustBlockTabHelper&) = delete;
  FramebustBlockTabHelper& operator=(const FramebustBlockTabHelper&) = delete;

  ~FramebustBlockTabHelper() override;

  // Shows the blocked Framebust icon in the Omnibox for the |blocked_url|.
  // If the icon is already visible, that URL is instead added to the vector of
  // currently blocked URLs and the bubble view is updated. The |click_callback|
  // will be called (if it is non-null) if the blocked URL is ever clicked.
  void AddBlockedUrl(const GURL& blocked_url, ClickCallback click_callback);

  // Returns true if at least one Framebust was blocked on this page.
  bool HasBlockedUrls() const;

  // Handles navigating to the blocked URL specified by |index| and clearing the
  // vector of blocked URLs.
  void OnBlockedUrlClicked(size_t index);

  // Returns all of the currently blocked URLs.
  const std::vector<GURL>& blocked_urls() const { return blocked_urls_; }

  blocked_content::UrlListManager* manager() { return &manager_; }

 private:
  friend class content::WebContentsUserData<FramebustBlockTabHelper>;

  explicit FramebustBlockTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  blocked_content::UrlListManager manager_;

  // Remembers all the currently blocked URLs. This is cleared on each
  // navigation.
  std::vector<GURL> blocked_urls_;

  // Callbacks associated with |blocked_urls_|. Separate vector to allow easy
  // distribution of the URLs in blocked_urls().
  std::vector<ClickCallback> callbacks_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_FRAMEBUST_BLOCK_TAB_HELPER_H_
