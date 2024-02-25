// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_base_feature.h"

#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"

namespace cronet {

using ::org::chromium::net::httpflags::BaseFeatureOverrides;

void ApplyBaseFeatureOverrides(const BaseFeatureOverrides& overrides) {
  if (base::FeatureList::GetInstance() != nullptr) {
    LOG(WARNING) << "Not setting Cronet base::Feature overrides as "
                    "base::Feature is already initialized";
    return;
  }

  // We need to ensure base::FieldTrialList is initialized, otherwise the call
  // to base::FieldTrialList::CreateFieldTrial() below will crash.
  {
    // Intentional leak (singleton). Note that we can't use a static
    // base::NoDestructor here because that would lead to the FieldTrialList
    // singleton not getting properly reset between unit tests.
    auto* const field_trial_list = new base::FieldTrialList();
    ANNOTATE_LEAKING_OBJECT_PTR(field_trial_list);
    (void)field_trial_list;
  }

  auto feature_list = std::make_unique<base::FeatureList>();
  for (const auto& [feature_name, feature_state] : overrides.feature_states()) {
    // Cronet uses its own bespoke metrics logging system, and never reports
    // base::FieldTrial data back to Finch. We still need to provide a
    // FieldTrial to be able to register the base::Feature override and to
    // associate params, so let's create a fake one with bogus names. This is
    // similar in principle to how Chrome base::Features can be overridden from
    // the command line, and in fact the naming scheme below is inspired by how
    // base::FeatureList::InitializeFromCommandLine() generates fake field trial
    // and group names, with an additional "Cronet" prefix.
    const std::string field_trial_name = "CronetStudy" + feature_name;
    const std::string field_trial_group = "CronetGroup" + feature_name;
    auto* const field_trial = base::FieldTrialList::CreateFieldTrial(
        field_trial_name, field_trial_group);
    CHECK(field_trial != nullptr)
        << "Unable to create field trial for feature: " << feature_name;
    feature_list->RegisterFieldTrialOverride(
        feature_name,
        feature_state.has_enabled()
            ? (feature_state.enabled()
                   ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                   : base::FeatureList::OVERRIDE_DISABLE_FEATURE)
            : base::FeatureList::OVERRIDE_USE_DEFAULT,
        field_trial);
    if (!feature_state.params().empty()) {
      base::AssociateFieldTrialParams(
          field_trial_name, field_trial_group,
          {feature_state.params().begin(), feature_state.params().end()});
    }
  }
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace cronet
