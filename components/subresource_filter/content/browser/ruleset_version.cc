// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ruleset_version.h"

#include "base/trace_event/traced_value.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"

namespace subresource_filter {

namespace {

// Names of the preferences storing the most recent ruleset version that
// was successfully stored to disk.
const char kSubresourceFilterRulesetContentVersion[] =
    "subresource_filter.ruleset_version.content";
const char kSubresourceFilterRulesetFormatVersion[] =
    "subresource_filter.ruleset_version.format";
const char kSubresourceFilterRulesetChecksum[] =
    "subresource_filter.ruleset_version.checksum";

}  // namespace

UnindexedRulesetInfo::UnindexedRulesetInfo() = default;
UnindexedRulesetInfo::~UnindexedRulesetInfo() = default;
UnindexedRulesetInfo::UnindexedRulesetInfo(const UnindexedRulesetInfo&) =
    default;
UnindexedRulesetInfo& UnindexedRulesetInfo::operator=(
    const UnindexedRulesetInfo&) = default;

IndexedRulesetVersion::IndexedRulesetVersion() = default;
IndexedRulesetVersion::IndexedRulesetVersion(const std::string& content_version,
                                             int format_version)
    : content_version(content_version), format_version(format_version) {}
IndexedRulesetVersion::~IndexedRulesetVersion() = default;
IndexedRulesetVersion& IndexedRulesetVersion::operator=(
    const IndexedRulesetVersion&) = default;

// static
void IndexedRulesetVersion::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kSubresourceFilterRulesetContentVersion,
                               std::string());
  registry->RegisterIntegerPref(kSubresourceFilterRulesetFormatVersion, 0);
  registry->RegisterIntegerPref(kSubresourceFilterRulesetChecksum, 0);
}

// static
int IndexedRulesetVersion::CurrentFormatVersion() {
  return RulesetIndexer::kIndexedFormatVersion;
}

void IndexedRulesetVersion::ReadFromPrefs(PrefService* local_state) {
  format_version =
      local_state->GetInteger(kSubresourceFilterRulesetFormatVersion);
  content_version =
      local_state->GetString(kSubresourceFilterRulesetContentVersion);
  checksum = local_state->GetInteger(kSubresourceFilterRulesetChecksum);
}

bool IndexedRulesetVersion::IsValid() const {
  return format_version != 0 && !content_version.empty();
}

bool IndexedRulesetVersion::IsCurrentFormatVersion() const {
  return format_version == CurrentFormatVersion();
}

void IndexedRulesetVersion::SaveToPrefs(PrefService* local_state) const {
  local_state->SetInteger(kSubresourceFilterRulesetFormatVersion,
                          format_version);
  local_state->SetString(kSubresourceFilterRulesetContentVersion,
                         content_version);
  local_state->SetInteger(kSubresourceFilterRulesetChecksum, checksum);
}

std::unique_ptr<base::trace_event::TracedValue>
IndexedRulesetVersion::ToTracedValue() const {
  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetString("content_version", content_version);
  value->SetInteger("format_version", format_version);
  return value;
}

}  // namespace subresource_filter
