// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/anchor_element_provider.h"

#include <utility>

#include "ui/base/interaction/element_identifier.h"

namespace user_education {

AnchorElementProviderCommon::AnchorElementProviderCommon() = default;

AnchorElementProviderCommon::AnchorElementProviderCommon(
    ui::ElementIdentifier anchor_element_id)
    : anchor_element_id_(anchor_element_id) {}

AnchorElementProviderCommon::AnchorElementProviderCommon(
    AnchorElementProviderCommon&& other) noexcept
    : anchor_element_id_(
          std::exchange(other.anchor_element_id_, ui::ElementIdentifier())),
      in_any_context_(std::exchange(other.in_any_context_, false)),
      anchor_element_filter_(std::move(other.anchor_element_filter_)) {}

AnchorElementProviderCommon& AnchorElementProviderCommon::operator=(
    AnchorElementProviderCommon&& other) noexcept {
  if (this != &other) {
    anchor_element_id_ =
        std::exchange(other.anchor_element_id_, ui::ElementIdentifier());
    in_any_context_ = std::exchange(other.in_any_context_, false);
    anchor_element_filter_ = std::move(other.anchor_element_filter_);
  }
  return *this;
}

AnchorElementProviderCommon::~AnchorElementProviderCommon() = default;

ui::TrackedElement* AnchorElementProviderCommon::GetAnchorElement(
    ui::ElementContext context) const {
  CHECK(anchor_element_id_)
      << "Cannot call GetAnchorElement on default-constructed object.";

  auto* const element_tracker = ui::ElementTracker::GetElementTracker();
  if (anchor_element_filter_) {
    return anchor_element_filter_.Run(
        in_any_context_ ? element_tracker->GetAllMatchingElementsInAnyContext(
                              anchor_element_id_)
                        : element_tracker->GetAllMatchingElements(
                              anchor_element_id_, context));
  } else {
    return in_any_context_
               ? element_tracker->GetElementInAnyContext(anchor_element_id_)
               : element_tracker->GetFirstMatchingElement(anchor_element_id_,
                                                          context);
  }
}

}  // namespace user_education
