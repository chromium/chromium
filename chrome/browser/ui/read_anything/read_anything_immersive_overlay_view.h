// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_OVERLAY_VIEW_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_OVERLAY_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class ReadAnythingImmersiveWebView;

// This view is an overlay that sits on top of the main web contents. It's
// used to house the UI for the Immersive Reading Mode feature, which provides
// a distraction-free reading mode.
class ReadAnythingImmersiveOverlayView : public views::View {
  METADATA_HEADER(ReadAnythingImmersiveOverlayView, views::View)

 public:
  ReadAnythingImmersiveOverlayView();
  ~ReadAnythingImmersiveOverlayView() override;

  ReadAnythingImmersiveOverlayView(const ReadAnythingImmersiveOverlayView&) =
      delete;
  ReadAnythingImmersiveOverlayView& operator=(
      const ReadAnythingImmersiveOverlayView&) = delete;

  void ShowUI(std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
                  contents_wrapper,
              ReadAnythingOpenTrigger trigger);

 private:
  raw_ptr<ReadAnythingImmersiveWebView> immersive_web_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_OVERLAY_VIEW_H_
