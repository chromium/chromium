// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RECOMMEND_APPS_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RECOMMEND_APPS_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class RecommendAppsScreen;

// Interface for dependency injection between RecommendAppsScreen and its
// WebUI representation.
class RecommendAppsScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"recommend-apps",
                                                       "RecommendAppsScreen"};

  virtual ~RecommendAppsScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Called when the download of the recommend app list is successful. Shows the
  // downloaded `app_list` to the user.
  virtual void OnLoadSuccess(base::Value app_list) = 0;

  // Called when parsing the recommend app list response fails. Should skip this
  // screen.
  virtual void OnParseResponseError() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<RecommendAppsScreenView> AsWeakPtr() = 0;
};

// The sole implementation of the RecommendAppsScreenView, using WebUI.
class RecommendAppsScreenHandler final : public BaseScreenHandler,
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

  // RecommendAppsScreenView:
  void Show() override;
  void OnLoadSuccess(base::Value app_list) override;
  void OnParseResponseError() override;
  base::WeakPtr<RecommendAppsScreenView> AsWeakPtr() override;

 private:
  // Call the JS function to load the list of apps in the WebView.
  void LoadAppListInUI(base::Value app_list);

  base::WeakPtrFactory<RecommendAppsScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RECOMMEND_APPS_SCREEN_HANDLER_H_
