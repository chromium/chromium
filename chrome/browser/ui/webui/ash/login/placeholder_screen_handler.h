// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PLACEHOLDER_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PLACEHOLDER_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class PlaceholderScreen;

// Interface for dependency injection between PlaceholderScreen and its
// WebUI representation.
class PlaceholderScreenView {
 public:
  // LINT.IfChange(UsageMetrics)
  inline constexpr static StaticOobeScreenId kScreenId{"placeholder",
                                                       "PlaceholderScreen"};
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)

  virtual ~PlaceholderScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<PlaceholderScreenView> AsWeakPtr() = 0;
};

class PlaceholderScreenHandler : public BaseScreenHandler,
                                 public PlaceholderScreenView {
 public:
  using TView = PlaceholderScreenView;

  PlaceholderScreenHandler();

  PlaceholderScreenHandler(const PlaceholderScreenHandler&) = delete;
  PlaceholderScreenHandler& operator=(const PlaceholderScreenHandler&) = delete;

  ~PlaceholderScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // PlaceholderScreenView:
  void Show() override;
  base::WeakPtr<PlaceholderScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<PlaceholderScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PLACEHOLDER_SCREEN_HANDLER_H_
