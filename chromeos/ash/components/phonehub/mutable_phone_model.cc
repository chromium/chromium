// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/mutable_phone_model.h"

namespace ash {
namespace phonehub {

MutablePhoneModel::MutablePhoneModel() = default;

MutablePhoneModel::~MutablePhoneModel() = default;

void MutablePhoneModel::SetPhoneName(
    const absl::optional<std::u16string>& phone_name) {
  if (phone_name_ == phone_name)
    return;

  phone_name_ = phone_name;
  NotifyModelChanged();
}

void MutablePhoneModel::SetPhoneStatusModel(
    const absl::optional<PhoneStatusModel>& phone_status_model) {
  if (phone_status_model_ == phone_status_model)
    return;

  phone_status_model_ = phone_status_model;
  NotifyModelChanged();
}

void MutablePhoneModel::SetBrowserTabsModel(
    const absl::optional<BrowserTabsModel>& browser_tabs_model) {
  if (browser_tabs_model_ == browser_tabs_model)
    return;

  browser_tabs_model_ = browser_tabs_model;
  NotifyModelChanged();
}

}  // namespace phonehub
}  // namespace ash
