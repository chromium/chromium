// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PACKAGED_LICENSE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PACKAGED_LICENSE_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace ash {
class PackagedLicenseScreen;
}

namespace chromeos {

class PackagedLicenseView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"packaged-license"};

  PackagedLicenseView() = default;
  PackagedLicenseView(const PackagedLicenseView&) = delete;
  PackagedLicenseView& operator=(const PackagedLicenseView&) = delete;
  virtual ~PackagedLicenseView() = default;

  // Binds `screen` to the view.
  virtual void Bind(ash::PackagedLicenseScreen* screen) = 0;

  // Unbinds model from the view.
  virtual void Unbind() = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;
};

// A class that handles WebUI hooks in PackagedLicense screen.
class PackagedLicenseScreenHandler : public BaseScreenHandler,
                                     public PackagedLicenseView {
 public:
  using TView = PackagedLicenseView;
  PackagedLicenseScreenHandler();
  PackagedLicenseScreenHandler(const PackagedLicenseScreenHandler&) = delete;
  PackagedLicenseScreenHandler& operator=(const PackagedLicenseScreenHandler&) =
      delete;
  ~PackagedLicenseScreenHandler() override;

  // PackagedLicenseView:
  void Bind(ash::PackagedLicenseScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

 private:
  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitializeDeprecated() override;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  ash::PackagedLicenseScreen* screen_ = nullptr;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::PackagedLicenseScreenHandler;
using ::chromeos::PackagedLicenseView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PACKAGED_LICENSE_SCREEN_HANDLER_H_
