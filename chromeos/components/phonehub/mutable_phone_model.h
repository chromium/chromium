// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_MUTABLE_PHONE_MODEL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_MUTABLE_PHONE_MODEL_H_

#include "chromeos/components/phonehub/phone_model.h"

namespace chromeos {
namespace phonehub {

// Phone model which provides public API functions allowing the model to be
// updated.
class MutablePhoneModel : public PhoneModel {
 public:
  MutablePhoneModel();
  ~MutablePhoneModel() override;

  void SetPhoneName(const base::Optional<base::string16>& phone_name);
  void SetPhoneStatusModel(
      const base::Optional<PhoneStatusModel>& phone_status_model);
  void SetBrowserTabsModel(
      const base::Optional<BrowserTabsModel>& browser_tabs_model);
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_MUTABLE_PHONE_MODEL_H_
