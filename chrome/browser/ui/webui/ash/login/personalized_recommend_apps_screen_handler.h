// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PERSONALIZED_RECOMMEND_APPS_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PERSONALIZED_RECOMMEND_APPS_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between PersonalizedRecommendAppsScreen and
// its WebUI representation.
class PersonalizedRecommendAppsScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "personalized-apps", "PersonalizedRecommendAppsScreen"};

  virtual ~PersonalizedRecommendAppsScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  virtual void SetAppsAndUseCasesData(base::Value::List useCasesApps) = 0;
  virtual void SetOverviewStep() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<PersonalizedRecommendAppsScreenView> AsWeakPtr() = 0;
};

class PersonalizedRecommendAppsScreenHandler
    : public BaseScreenHandler,
      public PersonalizedRecommendAppsScreenView {
 public:
  using TView = PersonalizedRecommendAppsScreenView;

  PersonalizedRecommendAppsScreenHandler();

  PersonalizedRecommendAppsScreenHandler(
      const PersonalizedRecommendAppsScreenHandler&) = delete;
  PersonalizedRecommendAppsScreenHandler& operator=(
      const PersonalizedRecommendAppsScreenHandler&) = delete;

  ~PersonalizedRecommendAppsScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // PersonalizedRecommendAppsScreenView:
  void Show() override;
  void SetAppsAndUseCasesData(base::Value::List useCasesApps) override;
  void SetOverviewStep() override;
  base::WeakPtr<PersonalizedRecommendAppsScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<PersonalizedRecommendAppsScreenView> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PERSONALIZED_RECOMMEND_APPS_SCREEN_HANDLER_H_
