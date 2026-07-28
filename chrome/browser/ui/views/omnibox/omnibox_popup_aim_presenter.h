// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_AIM_PRESENTER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_AIM_PRESENTER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "components/permissions/permission_request_manager.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class LocationBar;
class OmniboxController;
class OmniboxPopupPresenterDelegate;

// Implements subclass of OmniboxPopupPresenterBase to present the AI-Mode
// compose-plate into an Omnibox popup.
class OmniboxPopupAimPresenter
    : public OmniboxPopupPresenterBase,
      public views::WidgetObserver,
      public views::FocusChangeListener {
 public:
  OmniboxPopupAimPresenter(LocationBar* location_bar,
                           OmniboxController* controller,
                           OmniboxPopupPresenterDelegate& presenter_delegate);
  OmniboxPopupAimPresenter(const OmniboxPopupAimPresenter&) = delete;
  OmniboxPopupAimPresenter& operator=(const OmniboxPopupAimPresenter&) = delete;
  ~OmniboxPopupAimPresenter() override;

  // OmniboxPopupPresenterBase:
  void Show() override;
  void Hide() override;
  std::string_view GetPopupMetricPrefix() const override;
  std::optional<base::TimeDelta> ShouldDeferUntilVisualStateReady()
      const override;
  bool ShouldDetachWebContentsOnHide() const override;
  // Triggered when a file selection dialog opened by this popup is closed,
  // initiating the focus restoration flow.
  void OnFileSelectionClosed() override;

  bool is_restoring_focus_after_file_selection() const {
    return is_restoring_focus_after_file_selection_;
  }

 protected:
  // OmniboxPopupPresenterBase overrides:
  void LogResultToContentReadyMetric(base::TimeTicks result_ready_time,
                                     bool success) override;
  void WidgetDestroyed() override;

 private:
  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // views::FocusChangeListener:
  // Intercepts focus shifts while `is_restoring_focus_after_file_selection_` is
  // active. This prevents the omnibox popup from closing due to focus
  // transitions and allows focus to be restored to the popup WebUI after the
  // file picker closes.
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // Finishing step for focus restoration after file picker selection. Restores
  // focus to the popup WebUI and resets the focus restoration state.
  void FinishFocusRestoration();

  // Resets focus restoration state and stops observing `FocusManager`.
  void ResetFocusRestorationState();

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // Observes the browser window's FocusManager to track focus restoration after
  // the file selector closes.
  base::ScopedObservation<views::FocusManager, views::FocusChangeListener>
      focus_manager_observation_{this};

  // Set to true when the file selection dialog is closed to prevent the
  // omnibox popup from closing due to focus transitions during focus
  // restoration.
  bool is_restoring_focus_after_file_selection_ = false;

  base::WeakPtrFactory<OmniboxPopupAimPresenter> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_AIM_PRESENTER_H_
