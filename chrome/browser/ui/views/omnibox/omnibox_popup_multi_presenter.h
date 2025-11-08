// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_MULTI_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_MULTI_PRESENTER_H_

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

class LocationBarView;
class OmniboxController;

// Builds on the single WebUI OmniboxPopupPresenter to add a second WebUI which
// is displayed as the AI-Mode compose plate and fills the entire popup,
// covering the cutout.
class OmniboxPopupMultiPresenter : public OmniboxPopupPresenterBase {
 public:
  OmniboxPopupMultiPresenter(LocationBarView* location_bar_view,
                             OmniboxController* controller);
  OmniboxPopupMultiPresenter(const OmniboxPopupMultiPresenter&) = delete;
  OmniboxPopupMultiPresenter& operator=(const OmniboxPopupMultiPresenter&) =
      delete;
  ~OmniboxPopupMultiPresenter() override;

 protected:
  // OmniboxPopupPresenterBase overrides;
  void WidgetDestroyed() override;
  std::optional<size_t> GetShowingWebUIContentIndex() const override;
  void ShowWebUIContent(size_t index) override;

 private:
  // Index of the WebView content currently being shown.
  std::optional<size_t> webview_index_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_MULTI_PRESENTER_H_
