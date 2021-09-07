// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace client_hints {

// This class tracks the viewport size of the visible main frame. This value is
// used when calculating the viewport width and height on prefetch requests,
// since prefetch requests cannot directly query the render frame tree.
class ClientHintsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ClientHintsWebContentsObserver> {
 public:
  ClientHintsWebContentsObserver(const ClientHintsWebContentsObserver&) =
      delete;
  ClientHintsWebContentsObserver& operator=(
      const ClientHintsWebContentsObserver&) = delete;
  ~ClientHintsWebContentsObserver() override;

 private:
  explicit ClientHintsWebContentsObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ClientHintsWebContentsObserver>;

  // content::WebContentsObserver
  void OnVisibilityChanged(content::Visibility visibility) override;
  void MainFrameWasResized(bool width_changed) override;

  void UpdateVisibleMainFrameViewportSize();

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace client_hints

#endif  // COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_WEB_CONTENTS_OBSERVER_H_
