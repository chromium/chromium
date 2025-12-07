// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_CONTENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_CONTENT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

class LocationBarView;
class OmniboxPopupPresenterBase;

// The content WebView for the popup of a WebUI Omnibox.
class OmniboxPopupWebUIContent : public OmniboxPopupWebUIBaseContent {
  METADATA_HEADER(OmniboxPopupWebUIContent, OmniboxPopupWebUIBaseContent)

 public:
  OmniboxPopupWebUIContent() = delete;
  OmniboxPopupWebUIContent(OmniboxPopupPresenterBase* presenter,
                           LocationBarView* location_bar_view,
                           OmniboxController* controller,
                           bool include_location_bar_cutout,
                           bool wants_focus);
  OmniboxPopupWebUIContent(const OmniboxPopupWebUIContent&) = delete;
  OmniboxPopupWebUIContent& operator=(const OmniboxPopupWebUIContent&) = delete;
  ~OmniboxPopupWebUIContent() override;

  // WebUIContentsWrapper::Host:
  void ShowUI() override;

  bool include_location_bar_cutout() const { return !top_rounded_corners(); }

  bool wants_focus() const { return wants_focus_; }

 private:
  // Indicate whether this WebUI content wants to receive activation and focus.
  bool wants_focus_ = false;

  base::WeakPtrFactory<OmniboxPopupWebUIContent> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   OmniboxPopupWebUIContent,
                   OmniboxPopupWebUIBaseContent)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, OmniboxPopupWebUIContent)

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_WEBUI_CONTENT_H_
