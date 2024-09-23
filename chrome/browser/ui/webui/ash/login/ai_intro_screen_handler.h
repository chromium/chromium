// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_AI_INTRO_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_AI_INTRO_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class AiIntroScreen;

// Interface for dependency injection between AiIntroScreen and its
// WebUI representation.
class AiIntroScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"ai-intro",
                                                       "AiIntroScreen"};

  virtual ~AiIntroScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<AiIntroScreenView> AsWeakPtr() = 0;
};

class AiIntroScreenHandler final : public BaseScreenHandler,
                                    public AiIntroScreenView {
 public:
  using TView = AiIntroScreenView;

  AiIntroScreenHandler();

  AiIntroScreenHandler(const AiIntroScreenHandler&) = delete;
  AiIntroScreenHandler& operator=(const AiIntroScreenHandler&) = delete;

  ~AiIntroScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // AiIntroScreenView:
  void Show() override;
  base::WeakPtr<AiIntroScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<AiIntroScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_AI_INTRO_SCREEN_HANDLER_H_
