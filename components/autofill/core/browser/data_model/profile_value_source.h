// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PROFILE_VALUE_SOURCE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PROFILE_VALUE_SOURCE_H_

#include <string>
#include <vector>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Stores the (possible) source of address profile values found in a field at
// submission time.
struct ProfileValueSource {
  // The comparison operator will allow to easily remove duplicates.
  friend bool operator==(const ProfileValueSource&,
                         const ProfileValueSource&) = default;
  // An identifier (GUID) of an address profile.
  std::string identifier;
  // The type of the value found in the Autofill entry.
  FieldType value_type;
};

class PossibleProfileValueSources {
 public:
  PossibleProfileValueSources();
  PossibleProfileValueSources(const PossibleProfileValueSources&);
  ~PossibleProfileValueSources();

  void AddPossibleValueSource(std::string identifier, FieldType type);
  const std::vector<ProfileValueSource>& GetAllPossibleValueSources() const;
  void ClearAllPossibleValueSources();

 private:
  std::vector<ProfileValueSource> profile_value_sources_;
};

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PROFILE_VALUE_SOURCE_H_
