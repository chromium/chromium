// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/profile_value_source.h"

#include "components/autofill/core/browser/field_type_utils.h"

namespace autofill {

PossibleProfileValueSources::PossibleProfileValueSources() = default;
PossibleProfileValueSources::PossibleProfileValueSources(
    const PossibleProfileValueSources&) = default;
PossibleProfileValueSources::~PossibleProfileValueSources() = default;

void PossibleProfileValueSources::AddPossibleValueSource(std::string identifier,
                                                         FieldType type) {
  if (!IsAddressType(type)) {
    return;
  }
  profile_value_sources_.emplace_back(std::move(identifier), type);
}

void PossibleProfileValueSources::ClearAllPossibleValueSources() {
  profile_value_sources_.clear();
}

const std::vector<ProfileValueSource>&
PossibleProfileValueSources::GetAllPossibleValueSources() const {
  return profile_value_sources_;
}

}  // namespace autofill
