// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/user_education_metadata.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"

namespace user_education {

Metadata::Metadata(int launch_milestone_,
                   std::string owners_,
                   std::string additional_description_,
                   FeatureSet required_features_,
                   PlatformSet platforms_)
    : launch_milestone(launch_milestone_),
      owners(std::move(owners_)),
      additional_description(std::move(additional_description_)),
      required_features(std::move(required_features_)),
      platforms(std::move(platforms_)) {}

Metadata::Metadata() = default;
Metadata::Metadata(Metadata&&) noexcept = default;
Metadata& Metadata::operator=(Metadata&&) noexcept = default;
Metadata::~Metadata() = default;

}  // namespace user_education
