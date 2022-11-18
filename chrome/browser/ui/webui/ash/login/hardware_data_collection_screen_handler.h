// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_HARDWARE_DATA_COLLECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_HARDWARE_DATA_COLLECTION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface between HWDataCollection screen and its representation, either
// WebUI or Views one.
class HWDataCollectionView
    : public base::SupportsWeakPtr<HWDataCollectionView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "hw-data-collection", "HWDataCollectionScreen"};

  virtual ~HWDataCollectionView() = default;
  virtual void Show(bool enabled) = 0;
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
  void Show(bool enabled) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_HARDWARE_DATA_COLLECTION_SCREEN_HANDLER_H_
