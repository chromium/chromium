// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/help_bubble_params.h"

namespace user_education {

HelpBubbleButtonParams::HelpBubbleButtonParams() = default;
HelpBubbleButtonParams::HelpBubbleButtonParams(
    HelpBubbleButtonParams&&) noexcept = default;
HelpBubbleButtonParams::~HelpBubbleButtonParams() = default;
HelpBubbleButtonParams& HelpBubbleButtonParams::operator=(
    HelpBubbleButtonParams&&) noexcept = default;

HelpBubbleParams::ExtendedProperties::ExtendedProperties() = default;

HelpBubbleParams::ExtendedProperties::ExtendedProperties(
    const ExtendedProperties& other)
    : dict_(other.dict_.Clone()) {}

HelpBubbleParams::ExtendedProperties::ExtendedProperties(
    ExtendedProperties&& other) noexcept
    : dict_(std::move(other.dict_)) {}

HelpBubbleParams::ExtendedProperties&
HelpBubbleParams::ExtendedProperties::operator=(
    const ExtendedProperties& other) {
  dict_ = other.dict_.Clone();
  return *this;
}

HelpBubbleParams::ExtendedProperties&
HelpBubbleParams::ExtendedProperties::operator=(
    ExtendedProperties&& other) noexcept {
  dict_ = std::move(other.dict_);
  return *this;
}

bool HelpBubbleParams::ExtendedProperties::operator==(
    const ExtendedProperties& other) const {
  return dict_ == other.dict_;
}

bool HelpBubbleParams::ExtendedProperties::operator!=(
    const ExtendedProperties& other) const {
  return dict_ != other.dict_;
}

HelpBubbleParams::ExtendedProperties::~ExtendedProperties() = default;

HelpBubbleParams::HelpBubbleParams() = default;
HelpBubbleParams::HelpBubbleParams(HelpBubbleParams&&) noexcept = default;
HelpBubbleParams& HelpBubbleParams::operator=(HelpBubbleParams&&) noexcept =
    default;
HelpBubbleParams::~HelpBubbleParams() = default;

}  // namespace user_education
