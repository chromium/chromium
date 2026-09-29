// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"

namespace client_hints {

// This class tracks the viewport size of the visible main frame. This value is
// used when the viewport size is not directly available, such as for prefetch
// requests and for tab restore.
class ClientHintsWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit ClientHintsWebContentsObserver(content::WebContents* web_contents);
  ~ClientHintsWebContentsObserver() override;

  ClientHintsWebContentsObserver(const ClientHintsWebContentsObserver&) =
      delete;
  ClientHintsWebContentsObserver& operator=(
      const ClientHintsWebContentsObserver&) = delete;

  // content::WebContentsObserver
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryMainFrameWasResized(bool width_changed) override;

 private:
  void UpdateVisibleMainFrameViewportSize();
};

}  // namespace client_hints

#endif  // COMPONENTS_CLIENT_HINTS_BROWSER_CLIENT_HINTS_WEB_CONTENTS_OBSERVER_H_
