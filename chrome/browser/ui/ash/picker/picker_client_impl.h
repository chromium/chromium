// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_

#include "ash/public/cpp/picker/picker_client.h"

namespace ash {
class PickerController;
}

// Implements the PickerClient used by Ash.
class PickerClientImpl : public ash::PickerClient {
 public:
  // Sets this instance as the client of `controller`.
  // Automatically unsets the client when this instance is destroyed.
  explicit PickerClientImpl(ash::PickerController* controller);
  PickerClientImpl(const PickerClientImpl&) = delete;
  PickerClientImpl& operator=(const PickerClientImpl&) = delete;
  ~PickerClientImpl() override;

  // ash::PickerClient:
  std::unique_ptr<ash::AshWebView> CreateWebView(
      const ash::AshWebView::InitParams& params) override;

 private:
  raw_ptr<ash::PickerController> controller_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_
