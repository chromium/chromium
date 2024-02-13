// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PACKAGED_LICENSE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PACKAGED_LICENSE_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class PackagedLicenseView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"packaged-license",
                                                       "PackagedLicenseScreen"};

  PackagedLicenseView() = default;
  PackagedLicenseView(const PackagedLicenseView&) = delete;
  PackagedLicenseView& operator=(const PackagedLicenseView&) = delete;
  virtual ~PackagedLicenseView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<PackagedLicenseView> AsWeakPtr() = 0;
};

// A class that handles WebUI hooks in PackagedLicense screen.
class PackagedLicenseScreenHandler final : public BaseScreenHandler,
                                           public PackagedLicenseView {
 public:
  using TView = PackagedLicenseView;
  PackagedLicenseScreenHandler();
  PackagedLicenseScreenHandler(const PackagedLicenseScreenHandler&) = delete;
  PackagedLicenseScreenHandler& operator=(const PackagedLicenseScreenHandler&) =
      delete;
  ~PackagedLicenseScreenHandler() override;

  void Show() override;
  base::WeakPtr<PackagedLicenseView> AsWeakPtr() override;

 private:
  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<PackagedLicenseView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_PACKAGED_LICENSE_SCREEN_HANDLER_H_
