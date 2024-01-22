// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_FEATURE_VISITOR_H_
#define COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_FEATURE_VISITOR_H_

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/feature_visitor.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace variations::cros_early_boot::evaluate_seed {

// The EarlyBootFeatureVisitor class creates the computed state that will be
// output by the evaluate_seed binary.
class EarlyBootFeatureVisitor : public base::FeatureVisitor {
 public:
  EarlyBootFeatureVisitor();

  EarlyBootFeatureVisitor(const EarlyBootFeatureVisitor&) = delete;
  EarlyBootFeatureVisitor& operator=(const EarlyBootFeatureVisitor&) = delete;

  ~EarlyBootFeatureVisitor() override;

  // Creates a new featured::FeatureOverride message in `overrides_`.
  void Visit(const std::string& feature_name,
             const base::FeatureList::OverrideState override_state,
             const base::FieldTrialParams& params,
             const std::string& trial_name,
             const std::string& group_name) override;

  google::protobuf::RepeatedPtrField<featured::FeatureOverride>
  release_overrides() {
    return std::move(overrides_);
  }

 private:
  google::protobuf::RepeatedPtrField<featured::FeatureOverride> overrides_;
};

}  // namespace variations::cros_early_boot::evaluate_seed

#endif  // COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_EARLY_BOOT_FEATURE_VISITOR_H_
