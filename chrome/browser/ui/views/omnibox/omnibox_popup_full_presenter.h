// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_

#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class LocationBar;
class OmniboxPopupPresenterDelegate;
class OmniboxController;

// Implements subclass of OmniboxPopupPresenterBase to present a single full
// WebUI (input row + suggestions dropdown) into the Omnibox popup.
class OmniboxPopupFullPresenter : public OmniboxPopupPresenterBase {
 public:
  OmniboxPopupFullPresenter(LocationBar* location_bar,
                            OmniboxPopupPresenterDelegate& presenter_delegate,
                            OmniboxController* controller);
  OmniboxPopupFullPresenter(const OmniboxPopupFullPresenter&) = delete;
  OmniboxPopupFullPresenter& operator=(const OmniboxPopupFullPresenter&) =
      delete;
  ~OmniboxPopupFullPresenter() override;

  // OmniboxPopupPresenterBase:
  void Show() override;
  void Hide() override;

  std::string_view GetPopupMetricPrefix() const override;

  std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const override;
  bool ShouldDetachWebContentsOnHide() const override;

 protected:
  // OmniboxPopupPresenterBase:
  std::unique_ptr<RoundedOmniboxResultsFrame> CreateResultsFrame(
      std::unique_ptr<views::View> contents,
      LocationBar* location_bar,
      bool forward_mouse_events) override;
  void SynchronizePopupBounds() override;
  void WidgetDestroyed() override;

 private:
  void StopForwardingEvents();

  // Timer to stop forwarding events after a short delay.
  base::OneShotTimer forward_events_timer_;

  // Whether the "first shown" metrics have been logged at least once.
  bool logged_first_shown_metric_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_FULL_PRESENTER_H_
