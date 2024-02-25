// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {
namespace phonehub {

MutablePhoneModel::MutablePhoneModel() = default;

MutablePhoneModel::~MutablePhoneModel() = default;

void MutablePhoneModel::SetPhoneName(
    const std::optional<std::u16string>& phone_name) {
  if (phone_name_ == phone_name)
    return;

  phone_name_ = phone_name;
  NotifyModelChanged();
}

void MutablePhoneModel::SetPhoneStatusModel(
    const std::optional<PhoneStatusModel>& phone_status_model) {
  if (phone_status_model_ == phone_status_model) {
    PA_LOG(INFO) << "Skipping update PhoneStatusModel since new and old are "
                    "same. They are "
                 << (phone_status_model_.has_value() ? "not empty" : "empty");
    return;
  }
  PA_LOG(INFO) << "Updating phone status model";
  phone_status_model_ = phone_status_model;
  NotifyModelChanged();
}

void MutablePhoneModel::SetBrowserTabsModel(
    const std::optional<BrowserTabsModel>& browser_tabs_model) {
  if (browser_tabs_model_ == browser_tabs_model)
    return;

  browser_tabs_model_ = browser_tabs_model;
  NotifyModelChanged();
}

}  // namespace phonehub
}  // namespace ash
