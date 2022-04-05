// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HARDWARE_DATA_COLLECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HARDWARE_DATA_COLLECTION_SCREEN_HANDLER_H_

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "components/login/secure_module_util_chromeos.h"

namespace ash {
class HWDataCollectionScreen;
}

namespace base {
class DictionaryValue;
}

namespace chromeos {

// Interface between HWDataCollection screen and its representation, either
// WebUI or Views one. Note, do not forget to call OnViewDestroyed in the
// dtor.
class HWDataCollectionView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"hw-data-collection"};

  virtual ~HWDataCollectionView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Bind(ash::HWDataCollectionScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void ShowHWDataUsageLearnMore() = 0;
};

// WebUI implementation of HWDataCollectionView. It is used to interact
// with the HWDataCollection part of the JS page.
class HWDataCollectionScreenHandler : public HWDataCollectionView,
                                      public BaseScreenHandler {
 public:
  using TView = HWDataCollectionView;

  HWDataCollectionScreenHandler();

  HWDataCollectionScreenHandler(const HWDataCollectionScreenHandler&) = delete;
  HWDataCollectionScreenHandler& operator=(
      const HWDataCollectionScreenHandler&) = delete;

  ~HWDataCollectionScreenHandler() override;

  // HWDataCollectionView implementation:
  void Show() override;
  void Hide() override;
  void Bind(ash::HWDataCollectionScreen* screen) override;
  void Unbind() override;
  void ShowHWDataUsageLearnMore() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitializeDeprecated() override;

 private:
  ash::HWDataCollectionScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::HWDataCollectionScreenHandler;
using ::chromeos::HWDataCollectionView;
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HARDWARE_DATA_COLLECTION_SCREEN_HANDLER_H_
