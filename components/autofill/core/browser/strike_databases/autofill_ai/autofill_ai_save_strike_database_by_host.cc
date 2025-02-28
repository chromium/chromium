// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_host.h"

#include <string>
#include <string_view>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace autofill {

namespace {
// Used as a separator to create "(entity_type, host)" pairs.
constexpr char kHostSeparator = ';';
}  // namespace

std::string AutofillAiSaveStrikeDatabaseByHostTraits::HostFromId(
    const std::string& id) {
  std::optional<std::pair<std::string_view, std::string_view>> split_string =
      base::SplitStringOnce(id, kHostSeparator);
  if (!split_string) {
    return {};
  }
  return std::string(split_string->second);
}

std::string AutofillAiSaveStrikeDatabaseByHost::GetId(
    std::string_view entity_name,
    std::string_view host) {
  return base::JoinString({entity_name, host},
                          std::string_view(&kHostSeparator, 1));
}

}  // namespace autofill
