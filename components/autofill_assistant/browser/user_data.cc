// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/password_form.h"

namespace autofill_assistant {

LoginChoice::LoginChoice(const std::string& _identifier,
                         const std::string& _label,
                         const std::string& _sublabel,
                         const std::string& _sublabel_accessibility_hint,
                         int _preselect_priority,
                         const base::Optional<InfoPopupProto>& _info_popup)
    : identifier(_identifier),
      label(_label),
      sublabel(_sublabel),
      sublabel_accessibility_hint(_sublabel_accessibility_hint),
      preselect_priority(_preselect_priority),
      info_popup(_info_popup) {}
LoginChoice::LoginChoice(const LoginChoice& another) = default;
LoginChoice::~LoginChoice() = default;

UserData::UserData() = default;
UserData::~UserData() = default;

CollectUserDataOptions::CollectUserDataOptions() = default;
CollectUserDataOptions::~CollectUserDataOptions() = default;

}  // namespace autofill_assistant
