// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CONSUMER_UPDATE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CONSUMER_UPDATE_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between ConsumerUpdateScreen and its
// WebUI representation.
class ConsumerUpdateScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"consumer-update",
                                                       "ConsumerUpdateScreen"};
  virtual ~ConsumerUpdateScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;
  virtual base::WeakPtr<ConsumerUpdateScreenView> AsWeakPtr() = 0;
};

class ConsumerUpdateScreenHandler final : public BaseScreenHandler,
                                          public ConsumerUpdateScreenView {
 public:
  using TView = ConsumerUpdateScreenView;

  ConsumerUpdateScreenHandler();
  ConsumerUpdateScreenHandler(const ConsumerUpdateScreenHandler&) = delete;
  ConsumerUpdateScreenHandler& operator=(const ConsumerUpdateScreenHandler&) =
      delete;
  ~ConsumerUpdateScreenHandler() override;

  // ConsumerUpdateScreenView:
  void Show() override;
  base::WeakPtr<ConsumerUpdateScreenView> AsWeakPtr() override;

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<ConsumerUpdateScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CONSUMER_UPDATE_SCREEN_HANDLER_H_
