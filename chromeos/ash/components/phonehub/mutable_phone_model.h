// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_MUTABLE_PHONE_MODEL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_MUTABLE_PHONE_MODEL_H_

#include "chromeos/ash/components/phonehub/phone_model.h"

namespace ash {
namespace phonehub {

// Phone model which provides public API functions allowing the model to be
// updated.
class MutablePhoneModel : public PhoneModel {
 public:
  MutablePhoneModel();
  ~MutablePhoneModel() override;

  void SetPhoneName(const std::optional<std::u16string>& phone_name);
  void SetPhoneStatusModel(
      const std::optional<PhoneStatusModel>& phone_status_model);
  void SetBrowserTabsModel(
      const std::optional<BrowserTabsModel>& browser_tabs_model);
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_MUTABLE_PHONE_MODEL_H_
