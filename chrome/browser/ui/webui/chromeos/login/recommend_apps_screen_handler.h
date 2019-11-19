// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RECOMMEND_APPS_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RECOMMEND_APPS_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

class RecommendAppsScreen;

// Interface for dependency injection between RecommendAppsScreen and its
// WebUI representation.
class RecommendAppsScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"recommend-apps"};

  virtual ~RecommendAppsScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(RecommendAppsScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Called when the download of the recommend app list fails. Show an error
  // message to the user.
  virtual void OnLoadError() = 0;

  // Called when the download of the recommend app list is successful. Shows the
  // downloaded |app_list| to the user.
  virtual void OnLoadSuccess(const base::Value& app_list) = 0;

  // Called when parsing the recommend app list response fails. Should skip this
  // screen.
  virtual void OnParseResponseError() = 0;
};

// The sole implementation of the RecommendAppsScreenView, using WebUI.
class RecommendAppsScreenHandler : public BaseScreenHandler,
                                   public RecommendAppsScreenView {
 public:
  using TView = RecommendAppsScreenView;

  explicit RecommendAppsScreenHandler(JSCallsContainer* js_calls_container);
  ~RecommendAppsScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;

  // RecommendAppsScreenView:
  void Bind(RecommendAppsScreen* screen) override;
  void Show() override;
  void Hide() override;
  void OnLoadError() override;
  void OnLoadSuccess(const base::Value& app_list) override;
  void OnParseResponseError() override;

  // BaseScreenHandler:
  void Initialize() override;

 private:
  void OnUserSkip();

  // Call the JS function to load the list of apps in the WebView.
  void LoadAppListInUI(const base::Value& app_list);

  void HandleSkip();
  void HandleRetry();
  void HandleInstall(const base::ListValue* args);

  RecommendAppsScreen* screen_ = nullptr;

  PrefService* pref_service_;

  int recommended_app_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(RecommendAppsScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_RECOMMEND_APPS_SCREEN_HANDLER_H_
