// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/generation/password_requirements_spec_printer.h"

namespace autofill {

std::ostream& operator<<(
    std::ostream& out,
    const PasswordRequirementsSpec::CharacterClass& character_class) {
  out << "{";
  if (character_class.has_character_set())
    out << "character_set: \"" << character_class.character_set() << "\", ";
  if (character_class.has_min())
    out << "min: " << character_class.min() << ", ";
  if (character_class.has_max())
    out << "max: " << character_class.max() << ", ";
  out << "}";
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const PasswordRequirementsSpec& spec) {
  out << "{";
  if (spec.has_priority())
    out << "priority: " << spec.priority() << ", ";
  if (spec.has_spec_version())
    out << "spec_version: " << spec.spec_version() << ", ";
  if (spec.has_min_length())
    out << "min_length: " << spec.min_length() << ", ";
  if (spec.has_max_length())
    out << "max_length: " << spec.max_length() << ", ";
  if (spec.has_lower_case())
    out << "lower_case: " << spec.lower_case() << ", ";
  if (spec.has_upper_case())
    out << "upper_case: " << spec.upper_case() << ", ";
  if (spec.has_alphabetic())
    out << "alphabetic: " << spec.alphabetic() << ", ";
  if (spec.has_numeric())
    out << "numeric: " << spec.alphabetic() << ", ";
  if (spec.has_symbols())
    out << "symbols: " << spec.symbols() << ", ";
  out << "}";
  return out;
}

}  // namespace autofill
