// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/fake_usb_chooser_controller.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

FakeUsbChooserController::FakeUsbChooserController(int device_count)
    : ChooserController(u""), device_count_(device_count) {
  set_title_for_testing(l10n_util::GetStringFUTF16(
      IDS_USB_DEVICE_CHOOSER_PROMPT, u"example.com"));
}

std::u16string FakeUsbChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::u16string FakeUsbChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
FakeUsbChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_LOADING_LABEL),
      l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_LOADING_LABEL_TOOLTIP)};
}

size_t FakeUsbChooserController::NumOptions() const {
  return device_count_;
}

std::u16string FakeUsbChooserController::GetOption(size_t index) const {
  return base::ASCIIToUTF16(base::StringPrintf("Device #%zu", index));
}
