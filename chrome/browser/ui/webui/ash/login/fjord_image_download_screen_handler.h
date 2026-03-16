// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_IMAGE_DOWNLOAD_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_IMAGE_DOWNLOAD_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class FjordImageDownloadScreen;

// Interface for dependency injection between FjordImageDownloadScreen and its
// WebUI representation.
class FjordImageDownloadScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "fjord-image-download", "FjordImageDownloadScreen"};

  virtual ~FjordImageDownloadScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<FjordImageDownloadScreenView> AsWeakPtr() = 0;
};

class FjordImageDownloadScreenHandler final
    : public BaseScreenHandler,
      public FjordImageDownloadScreenView {
 public:
  using TView = FjordImageDownloadScreenView;

  FjordImageDownloadScreenHandler();
  FjordImageDownloadScreenHandler(const FjordImageDownloadScreenHandler&) =
      delete;
  FjordImageDownloadScreenHandler& operator=(
      const FjordImageDownloadScreenHandler&) = delete;
  ~FjordImageDownloadScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // FjordImageDownloadScreenView:
  void Show() override;
  base::WeakPtr<FjordImageDownloadScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<FjordImageDownloadScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_IMAGE_DOWNLOAD_SCREEN_HANDLER_H_
