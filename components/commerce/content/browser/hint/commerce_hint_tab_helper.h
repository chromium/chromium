// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CONTENT_BROWSER_HINT_COMMERCE_HINT_TAB_HELPER_H_
#define COMPONENTS_COMMERCE_CONTENT_BROWSER_HINT_COMMERCE_HINT_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace commerce_hint {

// Tab helper to observe commerce hints on browser side.
class CommerceHintTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CommerceHintTabHelper> {
 public:
  CommerceHintTabHelper(const CommerceHintTabHelper&) = delete;
  CommerceHintTabHelper& operator=(const CommerceHintTabHelper&) = delete;

  ~CommerceHintTabHelper() override;

  // content::WebContentsObserver implementation.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  friend class content::WebContentsUserData<CommerceHintTabHelper>;

  explicit CommerceHintTabHelper(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace commerce_hint

#endif  // COMPONENTS_COMMERCE_CONTENT_BROWSER_HINT_COMMERCE_HINT_TAB_HELPER_H_
