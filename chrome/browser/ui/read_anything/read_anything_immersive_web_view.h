// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_WEB_VIEW_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_WEB_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/views/controls/webview/webview.h"

class Profile;

namespace tabs {
class TabInterface;
}  // namespace tabs

// This is a WebView used to house the WebUI for the Immersive Reading Mode. It
// is owned and hosted by the ReadAnythingImmersiveOverlayView. The
// ReadAnythingImmersiveOverlayView cannot directly host Reading Mode itself
// because ReadAnythingImmersiveOverlayView is a regular View, whereas rendering
// WebContents requires a WebView. So this class is a simple wrapper around
// WebContents that is hosted by the ReadAnythingImmersiveOverlayView.
class ReadAnythingImmersiveWebView : public views::WebView,
                                     public WebUIContentsWrapper::Host {
  METADATA_HEADER(ReadAnythingImmersiveWebView, views::WebView)

 public:
  ReadAnythingImmersiveWebView(
      std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
          contents_wrapper,
      ReadAnythingOpenTrigger trigger);

  ~ReadAnythingImmersiveWebView() override;

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;

 private:
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
      contents_wrapper_;
  const ReadAnythingOpenTrigger trigger_;

  base::WeakPtrFactory<ReadAnythingImmersiveWebView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_WEB_VIEW_H_
