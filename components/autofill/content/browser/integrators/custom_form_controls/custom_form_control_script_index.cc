// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_index.h"

#include <string_view>
#include <utility>

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace autofill {

CustomFormControlScriptIndex::CompiledEntry::CompiledEntry(
    std::string host,
    std::unique_ptr<const re2::RE2> filter,
    std::string script)
    : host(std::move(host)),
      filter(std::move(filter)),
      script(std::move(script)) {}
CustomFormControlScriptIndex::CompiledEntry::CompiledEntry(CompiledEntry&&) =
    default;
CustomFormControlScriptIndex::CompiledEntry&
CustomFormControlScriptIndex::CompiledEntry::operator=(CompiledEntry&&) =
    default;
CustomFormControlScriptIndex::CompiledEntry::~CompiledEntry() = default;

CustomFormControlScriptIndex::CustomFormControlScriptIndex() = default;

CustomFormControlScriptIndex::~CustomFormControlScriptIndex() = default;

void CustomFormControlScriptIndex::SetEntries(std::vector<Entry> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.clear();
  for (Entry& entry : entries) {
    const bool is_script_valid =
        !entry.script.empty() &&
        entry.script.size() <= kMaxCustomFormControlScriptSizeBytes;
    url::CanonHostInfo host_info;
    std::string host = net::CanonicalizeHost(entry.host, &host_info);
    const bool is_host_valid =
        !host.empty() && !host.starts_with('.') && !host.ends_with('.') &&
        net::registry_controlled_domains::HostHasRegistryControlledDomain(
            host, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (!is_script_valid || !is_host_valid) {
      continue;
    }
    std::unique_ptr<const re2::RE2> filter;
    if (!entry.url_filter.empty()) {
      filter = std::make_unique<re2::RE2>(entry.url_filter);
      if (!filter->ok()) {
        continue;
      }
    }
    entries_.emplace_back(std::move(host), std::move(filter),
                          std::move(entry.script));
  }
}

std::optional<std::string> CustomFormControlScriptIndex::FindScript(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }
  const std::string_view path_for_request = url.PathForRequestPiece();
  for (const CompiledEntry& entry : entries_) {
    if (url.DomainIs(entry.host) &&
        (!entry.filter ||
         re2::RE2::PartialMatch(path_for_request, *entry.filter))) {
      return entry.script;
    }
  }
  return std::nullopt;
}

}  // namespace autofill
