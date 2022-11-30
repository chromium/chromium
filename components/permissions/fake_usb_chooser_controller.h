// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_FAKE_USB_CHOOSER_CONTROLLER_H_
#define COMPONENTS_PERMISSIONS_FAKE_USB_CHOOSER_CONTROLLER_H_

#include <string>

#include "components/permissions/chooser_controller.h"

// A subclass of permissions::ChooserController that pretends to be a USB device
// chooser for testing. The result should be visually similar to the real
// version of the dialog for interactive tests.
class FakeUsbChooserController : public permissions::ChooserController {
 public:
  explicit FakeUsbChooserController(int device_count);

  FakeUsbChooserController(const FakeUsbChooserController&) = delete;
  FakeUsbChooserController& operator=(const FakeUsbChooserController&) = delete;

  // permissions::ChooserController:
  std::u16string GetNoOptionsText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::pair<std::u16string, std::u16string> GetThrobberLabelAndTooltip()
      const override;
  size_t NumOptions() const override;
  std::u16string GetOption(size_t index) const override;
  void Select(const std::vector<size_t>& indices) override {}
  void Cancel() override {}
  void Close() override {}
  void OpenHelpCenterUrl() const override {}

  void set_device_count(size_t device_count) { device_count_ = device_count; }

 private:
  // The number of fake devices to include in the chooser. Names are generated
  // for them.
  size_t device_count_ = 0;
};

#endif  // COMPONENTS_PERMISSIONS_FAKE_USB_CHOOSER_CONTROLLER_H_
