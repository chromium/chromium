// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_VR_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_VR_VR_WEB_CONTENTS_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace vr {

class BrowserUiInterface;
class LocationBarHelper;

class VR_EXPORT VrWebContentsObserver : public content::WebContentsObserver {
 public:
  VrWebContentsObserver(content::WebContents* web_contents,
                        BrowserUiInterface* ui_interface,
                        LocationBarHelper* toolbar,
                        base::OnceClosure on_destroy);
  ~VrWebContentsObserver() override;

 private:
  // WebContentsObserver implementation.
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  void DidChangeVisibleSecurityState() override;
  void WebContentsDestroyed() override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  // This class does not own these pointers.
  BrowserUiInterface* ui_interface_;
  LocationBarHelper* toolbar_;

  base::OnceClosure on_destroy_;

  DISALLOW_COPY_AND_ASSIGN(VrWebContentsObserver);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_VR_WEB_CONTENTS_OBSERVER_H_
