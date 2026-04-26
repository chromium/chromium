// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_

#include <cstddef>
#include <optional>
#include <string_view>

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

class LocationBar;
class OmniboxController;

// Implements subclass of OmniboxPopupPresenterBase to present a single WebUI
// into the Omnibox popup.
class OmniboxPopupPresenter : public OmniboxPopupPresenterBase {
 public:
  OmniboxPopupPresenter(LocationBar* location_bar,
                        OmniboxPopupPresenterDelegate& presenter_delegate,
                        OmniboxController* controller);
  OmniboxPopupPresenter(const OmniboxPopupPresenter&) = delete;
  OmniboxPopupPresenter& operator=(const OmniboxPopupPresenter&) = delete;
  ~OmniboxPopupPresenter() override;

  void Hide() override;
  std::string_view GetPopupMetricPrefix() const override;

 protected:
  // OmniboxPopupPresenterBase overrides:
  void WidgetDestroyed() override;
  bool ShouldShowLocationBarCutout() const override;
  bool ShouldReceiveFocus() const override;
  std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const override;
  bool ShouldDetachWebContentsOnHide() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_H_
