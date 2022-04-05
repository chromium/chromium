// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RECOMMEND_APPS_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RECOMMEND_APPS_SCREEN_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "components/prefs/pref_service.h"

namespace ash {
class RecommendAppsScreen;
}

namespace chromeos {

// Interface for dependency injection between RecommendAppsScreen and its
// WebUI representation.
class RecommendAppsScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"recommend-apps-old"};

  virtual ~RecommendAppsScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(ash::RecommendAppsScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Called when the download of the recommend app list is successful. Shows the
  // downloaded `app_list` to the user.
  virtual void OnLoadSuccess(base::Value app_list) = 0;

  // Called when parsing the recommend app list response fails. Should skip this
  // screen.
  virtual void OnParseResponseError() = 0;
};

// The sole implementation of the RecommendAppsScreenView, using WebUI.
class RecommendAppsScreenHandler : public BaseScreenHandler,
                                   public RecommendAppsScreenView {
 public:
  using TView = RecommendAppsScreenView;

  RecommendAppsScreenHandler();

  RecommendAppsScreenHandler(const RecommendAppsScreenHandler&) = delete;
  RecommendAppsScreenHandler& operator=(const RecommendAppsScreenHandler&) =
      delete;

  ~RecommendAppsScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;

  // RecommendAppsScreenView:
  void Bind(ash::RecommendAppsScreen* screen) override;
  void Show() override;
  void Hide() override;
  void OnLoadSuccess(base::Value app_list) override;
  void OnParseResponseError() override;

  // BaseScreenHandler:
  void InitializeDeprecated() override;

 private:
  void OnUserSkip();

  // Call the JS function to load the list of apps in the WebView.
  void LoadAppListInUI(base::Value app_list);

  void HandleSkip();
  void HandleRetry();
  void HandleInstall(const base::Value::List& args);

  ash::RecommendAppsScreen* screen_ = nullptr;

  PrefService* pref_service_;

  int recommended_app_count_ = 0;

  // If true, InitializeDeprecated() will call Show().
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::RecommendAppsScreenHandler;
using ::chromeos::RecommendAppsScreenView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RECOMMEND_APPS_SCREEN_HANDLER_H_
