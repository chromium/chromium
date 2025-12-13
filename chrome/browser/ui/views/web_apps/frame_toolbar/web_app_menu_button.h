// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_MENU_BUTTON_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserView;

namespace web_app {
class WebAppRegistrar;
}

// The 'app menu' button for a web app window.
class WebAppMenuButton : public AppMenuButton,
                         public web_app::WebAppRegistrarObserver {
  METADATA_HEADER(WebAppMenuButton, AppMenuButton)

 public:
  static int GetMenuButtonSizeForBrowser(Browser* browser);
  explicit WebAppMenuButton(BrowserView* browser_view);
  WebAppMenuButton(const WebAppMenuButton&) = delete;
  WebAppMenuButton& operator=(const WebAppMenuButton&) = delete;
  ~WebAppMenuButton() override;

  // Fades the menu button highlight on and off.
  void StartHighlightAnimation();

  virtual void ButtonPressed(const ui::Event& event);

  bool IsLabelPresentAndVisible() const;

  // WebAppRegistrarObserver:
  void OnWebAppPendingUpdateChanged(const webapps::AppId& app_id,
                                    bool has_pending_update) override;
  void OnAppRegistrarDestroyed() override;

  // Causes this button to re-evaluate if a text label should be displayed
  // alongside the three-dot icon. Currently only exposed for tests, but
  // eventually production code needs to trigger something like this as well
  // when the update available state changes.
  void UpdateStateForTesting();

  // Shows the app menu. |run_types| denotes the MenuRunner::RunTypes associated
  // with the menu.
  void ShowMenu(int run_types);

  // Safely waits for the label text to be updated, as per the contracts of
  // `base::CallbackListSubscription`. Currently only used by tests, but can be
  // used to listen to dynamic label text updates.
  base::CallbackListSubscription AwaitLabelTextUpdated(
      base::RepeatingClosure callback);

 protected:
  BrowserView* browser_view() { return browser_view_; }

  // ToolbarButton:
  void OnThemeChanged() override;
  std::optional<SkColor> GetHighlightTextColor() const override;
  SkColor GetForegroundColor(ButtonState state) const override;
  int GetIconSize() const override;

  virtual std::optional<std::u16string> GetAccessibleNameOverride() const;

 private:
  void FadeHighlightOff();

  // Determines whether a pending update is available and can be shown on the
  // UX, based on whether the update is new and hasn't been ignored by the user.
  bool CanShowPendingUpdate();

  void UpdateTextAndHighlightColor(bool is_pending_update);

  // The containing browser view.
  raw_ptr<BrowserView> browser_view_;
  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::WebAppRegistrarObserver>
      registrar_observation_{this};

  base::OneShotTimer highlight_off_timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_MENU_BUTTON_H_
