// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_

#include <set>
#include <string>

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"

namespace variations {

// Decodes the variations header and extracts the variation ids.
bool ExtractVariationIds(const std::string& variations,
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids);

// Creates FieldTrial from given |key| and |id|.
scoped_refptr<base::FieldTrial> CreateTrialAndAssociateId(
    const std::string& trial_name,
    const std::string& default_group_name,
    IDCollectionKey key,
    VariationID id);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_
