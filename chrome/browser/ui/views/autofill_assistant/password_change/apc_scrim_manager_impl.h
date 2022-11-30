// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_IMPL_H_

#include <memory>

#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill_assistant/password_change/apc_scrim_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace content {
class WebContents;
}  // namespace content

class Browser;

// Implementation of `ApcScrimManager` interface.
class ApcScrimManagerImpl : public ApcScrimManager,
                            public content::WebContentsObserver,
                            public views::ViewObserver {
 public:
  explicit ApcScrimManagerImpl(content::WebContents* web_contents);
  ApcScrimManagerImpl(const ApcScrimManagerImpl&) = delete;
  ApcScrimManagerImpl& operator=(const ApcScrimManagerImpl&) = delete;

  ~ApcScrimManagerImpl() override;

  void Show() override;
  void Hide() override;
  void ShutDown() override;
  bool GetVisible() const override;
  void SetIsDisabled(bool is_disabled) override;
  bool GetIsDisabled() const override;
  void SetRestoreAccessibilityModeTimerForTest(
      std::unique_ptr<base::OneShotTimer> restore_accessibility_mode_timer);

 protected:
  // content::WebContentsObserver:
  // Protected so that it can be overridden by test class.
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Focus on the current webcontents.
  // Protected so that it can be overridden by the test class.
  virtual void FocusOnWebContents();

 private:
  // views::ViewObserver:
  // Updates the bounds of the `WebContents` overlay.
  void OnViewBoundsChanged(views::View* observed_view) override;

  void RestartOriginalAccessibilityMode(bool focus_on_web_contents);

  // Whether the scrim was visible before a webcontents visibility change.
  // This dictates if the scrim should be shown again or not.
  bool scrim_visible_on_webcontents_hide_ = false;

  // Returns the view of the `WebContents`.
  raw_ptr<views::View> GetContentsWebView();
  // Creates the `WebContents` overlay, so the user cannot click on the `WebContents`.
  std::unique_ptr<views::View> CreateOverlayView();

  // Whether the scrim manager has been disabled. A disabled scrim manager can
  // no longer be visible.
  bool is_disabled_ = false;
  base::ScopedObservation<views::View, ViewObserver> observation_{this};
  raw_ptr<views::View> overlay_view_ref_ = nullptr;
  raw_ptr<Browser> browser_;
  std::unique_ptr<base::OneShotTimer> restore_accessibility_mode_timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_SCRIM_MANAGER_IMPL_H_
