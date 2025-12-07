// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_AIM_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_AIM_PRESENTER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class LocationBarView;
class OmniboxController;

// Implements subclass of OmniboxPopupPresenterBase to present the AI-Mode
// compose-plate into an Omnibox popup.
class OmniboxPopupAimPresenter : public OmniboxPopupPresenterBase,
                                 public views::WidgetObserver {
 public:
  OmniboxPopupAimPresenter(LocationBarView* location_bar_view,
                           OmniboxController* controller);
  OmniboxPopupAimPresenter(const OmniboxPopupAimPresenter&) = delete;
  OmniboxPopupAimPresenter& operator=(const OmniboxPopupAimPresenter&) = delete;
  ~OmniboxPopupAimPresenter() override;

  // OmniboxPopupPresenterBase:
  void Show() override;
  void Hide() override;

 protected:
  // OmniboxPopupPresenterBase overrides:
  void WidgetDestroyed() override;

 private:
  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  const raw_ptr<OmniboxController> controller_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_AIM_PRESENTER_H_
