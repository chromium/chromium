// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/ruleset_version.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/trace_event/traced_value.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"

namespace subresource_filter {

namespace {

// Suffixes of names of the preferences storing the most recent ruleset versions
// that were successfully stored to disk.
const char kRulesetContentVersionSuffix[] = ".ruleset_version.content";
const char kRulesetFormatVersionSuffix[] = ".ruleset_version.format";
const char kRulesetChecksumSuffix[] = ".ruleset_version.checksum";

std::string ContentVersionPrefName(std::string_view filter_tag) {
  return base::StrCat({filter_tag, kRulesetContentVersionSuffix});
}

std::string FormatVersionPrefName(std::string_view filter_tag) {
  return base::StrCat({filter_tag, kRulesetFormatVersionSuffix});
}

std::string ChecksumPrefName(std::string_view filter_tag) {
  return base::StrCat({filter_tag, kRulesetChecksumSuffix});
}

}  // namespace

UnindexedRulesetInfo::UnindexedRulesetInfo() = default;
UnindexedRulesetInfo::~UnindexedRulesetInfo() = default;
UnindexedRulesetInfo::UnindexedRulesetInfo(const UnindexedRulesetInfo&) =
    default;
UnindexedRulesetInfo& UnindexedRulesetInfo::operator=(
    const UnindexedRulesetInfo&) = default;

IndexedRulesetVersion::IndexedRulesetVersion(std::string_view filter_tag)
    : filter_tag(std::string(filter_tag)) {}
// TODO(crbug.com/40280666): Convert |content_version| and |filter_tag| to
// std::string_view.
IndexedRulesetVersion::IndexedRulesetVersion(std::string_view content_version,
                                             int format_version,
                                             std::string_view filter_tag)
    : content_version(std::string(content_version)),
      format_version(format_version),
      filter_tag(std::string(filter_tag)) {}
IndexedRulesetVersion::~IndexedRulesetVersion() = default;
IndexedRulesetVersion& IndexedRulesetVersion::operator=(
    const IndexedRulesetVersion&) = default;

// static
void IndexedRulesetVersion::RegisterPrefs(PrefRegistrySimple* registry,
                                          std::string_view filter_tag) {
  registry->RegisterStringPref(ContentVersionPrefName(filter_tag),
                               std::string());
  registry->RegisterIntegerPref(FormatVersionPrefName(filter_tag), 0);
  registry->RegisterIntegerPref(ChecksumPrefName(filter_tag), 0);
}

// static
int IndexedRulesetVersion::CurrentFormatVersion() {
  return RulesetIndexer::kIndexedFormatVersion;
}

void IndexedRulesetVersion::ReadFromPrefs(PrefService* local_state) {
  content_version = local_state->GetString(ContentVersionPrefName(filter_tag));
  format_version = local_state->GetInteger(FormatVersionPrefName(filter_tag));
  checksum = local_state->GetInteger(ChecksumPrefName(filter_tag));
}

bool IndexedRulesetVersion::IsValid() const {
  return format_version != 0 && !content_version.empty();
}

bool IndexedRulesetVersion::IsCurrentFormatVersion() const {
  return format_version == CurrentFormatVersion();
}

void IndexedRulesetVersion::SaveToPrefs(PrefService* local_state) const {
  local_state->SetString(ContentVersionPrefName(filter_tag), content_version);
  local_state->SetInteger(FormatVersionPrefName(filter_tag), format_version);
  local_state->SetInteger(ChecksumPrefName(filter_tag), checksum);
}

std::unique_ptr<base::trace_event::TracedValue>
IndexedRulesetVersion::ToTracedValue() const {
  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetString("content_version", content_version);
  value->SetInteger("format_version", format_version);
  value->SetString("filter_tag", filter_tag);
  return value;
}

}  // namespace subresource_filter
