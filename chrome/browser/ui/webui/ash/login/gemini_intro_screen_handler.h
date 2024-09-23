// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GEMINI_INTRO_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GEMINI_INTRO_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class GeminiIntroScreen;

// Interface for dependency injection between GeminiIntroScreen and its
// WebUI representation.
class GeminiIntroScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"gemini-intro",
                                                       "GeminiIntroScreen"};

  virtual ~GeminiIntroScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(base::Value::Dict data) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<GeminiIntroScreenView> AsWeakPtr() = 0;
};

class GeminiIntroScreenHandler final : public BaseScreenHandler,
                                       public GeminiIntroScreenView {
 public:
  using TView = GeminiIntroScreenView;

  GeminiIntroScreenHandler();

  GeminiIntroScreenHandler(const GeminiIntroScreenHandler&) = delete;
  GeminiIntroScreenHandler& operator=(const GeminiIntroScreenHandler&) = delete;

  ~GeminiIntroScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // GeminiIntroScreenView:
  void Show(base::Value::Dict data) override;
  base::WeakPtr<GeminiIntroScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<GeminiIntroScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_GEMINI_INTRO_SCREEN_HANDLER_H_
