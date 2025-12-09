// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_AIM_POPUP_WEBUI_CONTENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_AIM_POPUP_WEBUI_CONTENT_H_

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

class LocationBarView;
class OmniboxController;
class OmniboxPopupAimHandler;
class OmniboxPopupPresenterBase;

// The content WebView for the popup of a WebUI Omnibox.
class OmniboxAimPopupWebUIContent : public OmniboxPopupWebUIBaseContent {
  METADATA_HEADER(OmniboxAimPopupWebUIContent, OmniboxPopupWebUIBaseContent)

 public:
  OmniboxAimPopupWebUIContent() = delete;
  OmniboxAimPopupWebUIContent(OmniboxPopupPresenterBase* presenter,
                              LocationBarView* location_bar_view,
                              OmniboxController* controller);
  OmniboxAimPopupWebUIContent(const OmniboxAimPopupWebUIContent&) = delete;
  OmniboxAimPopupWebUIContent& operator=(const OmniboxAimPopupWebUIContent&) =
      delete;
  ~OmniboxAimPopupWebUIContent() override;

  // OmniboxPopupWebUIBaseContent:
  // Called from the browser after widget has already closed. Will notify page
  // handler and WebUI.
  void OnWidgetClosed() override;

  // Called from page handler after `OnWidgetClosed()` notified it. `input` is
  // the possibly empty input that should replace the omnibox text.
  void OnPageClosedWithInput(const std::string& input);

 private:
  // WebUIContentsWrapper::Host:
  // Called from WebUI code to close the widget. I.e. when user presses
  // <escape>, presses the 'x' button, or moves focus out of the popup.
  void CloseUI() override;
  void ShowUI() override;

  // Can return null.
  OmniboxPopupAimHandler* popup_aim_handler();
};

BEGIN_VIEW_BUILDER(/* no export */,
                   OmniboxAimPopupWebUIContent,
                   OmniboxPopupWebUIBaseContent)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, OmniboxAimPopupWebUIContent)

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_AIM_POPUP_WEBUI_CONTENT_H_
