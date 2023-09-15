// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_base_feature.h"

#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"

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
  for (const auto& [feature_name, enable] : overrides.overrides()) {
    // Cronet uses its own bespoke metrics logging system, and never reports
    // base::FieldTrial data back to Finch. We still need to provide a
    // FieldTrial to be able to register the base::Feature override though, so
    // let's create a fake one with bogus names. This is similar in principle
    // to how Chrome base::Features can be overridden from the command line, and
    // in fact the naming scheme below is inspired by how
    // base::FeatureList::InitializeFromCommandLine() generates fake field trial
    // and group names, with an additional "Cronet" prefix.
    auto* const field_trial = base::FieldTrialList::CreateFieldTrial(
        "CronetStudy" + feature_name, "CronetGroup" + feature_name);
    CHECK(field_trial != nullptr)
        << "Unable to create field trial for feature: " << feature_name;
    feature_list->RegisterFieldTrialOverride(
        feature_name,
        enable ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
               : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
        field_trial);
  }
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace cronet
