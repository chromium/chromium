// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PERKS_DISCOVERY_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PERKS_DISCOVERY_SCREEN_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/perks_discovery_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between PerksDiscoveryScreen and
// its WebUI representation.
class PerksDiscoveryScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "perks-discovery", "PerksDiscoveryScreenScreen"};

  virtual ~PerksDiscoveryScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  virtual void SetPerksData(const std::vector<SinglePerkDiscoveryPayload>& perks) = 0;
  virtual void SetOverviewStep() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<PerksDiscoveryScreenView> AsWeakPtr() = 0;
};

class PerksDiscoveryScreenHandler : public BaseScreenHandler,
                                    public PerksDiscoveryScreenView {
 public:
  using TView = PerksDiscoveryScreenView;

  PerksDiscoveryScreenHandler();

  PerksDiscoveryScreenHandler(const PerksDiscoveryScreenHandler&) = delete;
  PerksDiscoveryScreenHandler& operator=(const PerksDiscoveryScreenHandler&) =
      delete;

  ~PerksDiscoveryScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // PerksDiscoveryScreenView:
  void Show() override;
  void SetPerksData(const std::vector<SinglePerkDiscoveryPayload>& perks) override;
  void SetOverviewStep() override;
  base::WeakPtr<PerksDiscoveryScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<PerksDiscoveryScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PERKS_DISCOVERY_SCREEN_HANDLER_H_
