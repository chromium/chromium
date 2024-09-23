// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_POPUP_NAVIGATION_DELEGATE_H_
#define COMPONENTS_BLOCKED_CONTENT_POPUP_NAVIGATION_DELEGATE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom-forward.h"
#include "ui/base/window_open_disposition.h"

class GURL;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace blocked_content {

// A delegate interface to allow an embedder specific representation of a
// navigation. This is stored internally in the popup blocker to recover
// navigations when the user clicks through a previously blocked popup.
class PopupNavigationDelegate {
 public:
  virtual ~PopupNavigationDelegate() = default;

  // Gets the opener used if new WebContents are created for this navigation.
  virtual content::RenderFrameHost* GetOpener() = 0;

  // Gets whether the blocked navigation was initiated by a user gesture.
  virtual bool GetOriginalUserGesture() = 0;

  // Gets the URL to be loaded.
  virtual GURL GetURL() = 0;

  // Performs the navigation.
  struct NavigateResult {
    raw_ptr<content::WebContents, DanglingUntriaged>
        navigated_or_inserted_contents = nullptr;
    WindowOpenDisposition disposition = WindowOpenDisposition::UNKNOWN;
  };
  virtual NavigateResult NavigateWithGesture(
      const blink::mojom::WindowFeatures& window_features,
      std::optional<WindowOpenDisposition> updated_disposition) = 0;

  // Called when the navigation represented by this class was blocked.
  virtual void OnPopupBlocked(content::WebContents* web_contents,
                              int total_popups_blocked_on_page) = 0;
};

}  // namespace blocked_content

#endif  // COMPONENTS_BLOCKED_CONTENT_POPUP_NAVIGATION_DELEGATE_H_
