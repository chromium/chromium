// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CONTENT_BROWSER_COMMERCE_TAB_HELPER_H_
#define COMPONENTS_COMMERCE_CONTENT_BROWSER_COMMERCE_TAB_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/commerce/content/browser/web_contents_wrapper.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/web_wrapper.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace commerce {

// This tab helper creates and maintains a WebWrapper that is backed by
// WebContents. Events that occur on the wrapper are reported back to the
// shopping service where they are used by various commerce features.
class CommerceTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CommerceTabHelper> {
 public:
  ~CommerceTabHelper() override;
  CommerceTabHelper(const CommerceTabHelper& other) = delete;
  CommerceTabHelper& operator=(const CommerceTabHelper& other) = delete;

  // content::WebContentsObserver implementation
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void DidStopLoading() override;

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  void OnWebContentsFocused(content::RenderWidgetHost* host) override;

  void WebContentsDestroyed() override;

  void SetShoppingServiceForTesting(KeyedService* service);

 private:
  friend class CommerceTabHelperTest;
  friend class content::WebContentsUserData<CommerceTabHelper>;

  CommerceTabHelper(content::WebContents* contents,
                    bool is_off_the_record,
                    ShoppingService* shopping_service,
                    int32_t js_world_id);

  const bool is_off_the_record_;

  std::unique_ptr<WebContentsWrapper> web_wrapper_;

  raw_ptr<ShoppingService, DanglingUntriaged> shopping_service_;

  // The url from the previous successful main frame navigation. This will be
  // empty if this is the first navigation for this tab or post-restart. We keep
  // track of this because the URL kepkt by the backing WebContents will have
  // changed before we get the signal for it.
  GURL previous_main_frame_url_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CONTENT_BROWSER_COMMERCE_TAB_HELPER_H_
