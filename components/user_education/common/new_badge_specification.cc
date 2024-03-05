// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/new_badge_specification.h"

#include <utility>

#include "base/feature_list.h"
#include "components/user_education/common/user_education_metadata.h"

namespace user_education {

NewBadgeSpecification::NewBadgeSpecification() = default;
NewBadgeSpecification::NewBadgeSpecification(const base::Feature& feature_,
                                             Metadata metadata_)
    : feature(&feature_), metadata(std::move(metadata_)) {}
NewBadgeSpecification::NewBadgeSpecification(NewBadgeSpecification&&) noexcept =
    default;
NewBadgeSpecification& NewBadgeSpecification::operator=(
    NewBadgeSpecification&&) noexcept = default;
NewBadgeSpecification::~NewBadgeSpecification() = default;

}  // namespace user_education
