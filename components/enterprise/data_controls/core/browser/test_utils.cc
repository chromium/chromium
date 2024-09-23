// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/test_utils.h"

#include "base/json/json_reader.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace data_controls {

void SetDataControls(PrefService* prefs,
                     std::vector<std::string> rules,
                     bool machine_scope) {
  ScopedListPrefUpdate list(prefs, kDataControlsRulesPref);
  if (!list->empty()) {
    list->clear();
  }

  for (const std::string& rule : rules) {
    list->Append(*base::JSONReader::Read(rule));
  }

  prefs->SetInteger(
      kDataControlsRulesScopePref,
      machine_scope ? policy::POLICY_SCOPE_MACHINE : policy::POLICY_SCOPE_USER);
}

}  // namespace data_controls
