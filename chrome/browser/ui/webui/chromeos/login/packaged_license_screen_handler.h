// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PACKAGED_LICENSE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PACKAGED_LICENSE_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class PackagedLicenseScreen;

class PackagedLicenseView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"packaged-license"};

  PackagedLicenseView() = default;
  PackagedLicenseView(const PackagedLicenseView&) = delete;
  PackagedLicenseView& operator=(const PackagedLicenseView&) = delete;
  virtual ~PackagedLicenseView() = default;

  // Binds `screen` to the view.
  virtual void Bind(PackagedLicenseScreen* screen) = 0;

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
  explicit PackagedLicenseScreenHandler(JSCallsContainer* js_calls_container);
  PackagedLicenseScreenHandler(const PackagedLicenseScreenHandler&) = delete;
  PackagedLicenseScreenHandler& operator=(const PackagedLicenseScreenHandler&) =
      delete;
  ~PackagedLicenseScreenHandler() override;

  // PackagedLicenseView:
  void Bind(PackagedLicenseScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

 private:
  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  PackagedLicenseScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_PACKAGED_LICENSE_SCREEN_HANDLER_H_
