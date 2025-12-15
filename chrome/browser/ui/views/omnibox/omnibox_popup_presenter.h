// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_

#include <cstddef>
#include <optional>

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

class LocationBarView;
class OmniboxController;

// Implements subclass of OmniboxPopupPresenterBase to present a single WebUI
// into the Omnibox popup.
class OmniboxPopupPresenter : public OmniboxPopupPresenterBase {
 public:
  OmniboxPopupPresenter(LocationBarView* location_bar_view,
                        OmniboxController* controller);
  OmniboxPopupPresenter(const OmniboxPopupPresenter&) = delete;
  OmniboxPopupPresenter& operator=(const OmniboxPopupPresenter&) = delete;
  ~OmniboxPopupPresenter() override;

  void Hide() override;

 protected:
  // OmniboxPopupPresenterBase overrides:
  void WidgetDestroyed() override;
  bool ShouldShowLocationBarCutout() const override;
  bool ShouldReceiveFocus() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
