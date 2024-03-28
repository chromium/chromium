// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TUNA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TUNA_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class TunaScreen;

// Interface for dependency injection between TunaScreen and its
// WebUI representation.
class TunaScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"tuna",
                                                       "TunaScreen"};

  virtual ~TunaScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(base::Value::Dict data) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<TunaScreenView> AsWeakPtr() = 0;
};

class TunaScreenHandler final : public BaseScreenHandler,
                                public TunaScreenView {
 public:
  using TView = TunaScreenView;

  TunaScreenHandler();

  TunaScreenHandler(const TunaScreenHandler&) = delete;
  TunaScreenHandler& operator=(const TunaScreenHandler&) = delete;

  ~TunaScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // TunaScreenView:
  void Show(base::Value::Dict data) override;
  base::WeakPtr<TunaScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<TunaScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TUNA_SCREEN_HANDLER_H_
