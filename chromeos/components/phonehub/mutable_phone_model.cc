// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/mutable_phone_model.h"

namespace chromeos {
namespace phonehub {

MutablePhoneModel::MutablePhoneModel() = default;

MutablePhoneModel::~MutablePhoneModel() = default;

void MutablePhoneModel::SetPhoneName(
    const base::Optional<base::string16>& phone_name) {
  if (phone_name_ == phone_name)
    return;

  phone_name_ = phone_name;
  NotifyModelChanged();
}

void MutablePhoneModel::SetPhoneStatusModel(
    const base::Optional<PhoneStatusModel>& phone_status_model) {
  if (phone_status_model_ == phone_status_model)
    return;

  phone_status_model_ = phone_status_model;
  NotifyModelChanged();
}

void MutablePhoneModel::SetBrowserTabsModel(
    const base::Optional<BrowserTabsModel>& browser_tabs_model) {
  if (browser_tabs_model_ == browser_tabs_model)
    return;

  browser_tabs_model_ = browser_tabs_model;
  NotifyModelChanged();
}

}  // namespace phonehub
}  // namespace chromeos
