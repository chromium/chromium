// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_TEST_UTILS_H_

#include <set>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"

namespace variations {

// The test seed data is associated with a VariationsSeed with one study,
// "UMA-Uniformity-Trial-10-Percent", and ten equally weighted groups: "default"
// and "group_01" through "group_09". The study is not associated with channels,
// platforms, or features.
//
// The seed and signature pair were generated using the server's private key.
extern const char kUncompressedBase64TestSeedData[];
extern const char kCompressedBase64TestSeedData[];
extern const char kBase64TestSeedSignature[];
extern const char kTestSeedStudyName[];

// Disables the use of the field trial testing config to exercise
// VariationsFieldTrialCreator::CreateTrialsFromSeed().
void DisableTestingConfig();

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
