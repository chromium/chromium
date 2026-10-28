// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_index.h"

#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace autofill {

namespace {

// Reads the entries listed in `directory`/index.json (see
// `CustomFormControlScriptIndex::InitializeFromDirectory()`). Returns nullopt
// if index.json can't be read or isn't a JSON list. Must run on a sequence that
// may block.
std::optional<std::vector<CustomFormControlScriptIndex::Entry>>
ReadEntriesFromDirectory(const base::FilePath& directory) {
  std::string json;
  if (!base::ReadFileToString(directory.AppendASCII("index.json"), &json)) {
    return std::nullopt;
  }
  std::optional<base::ListValue> list =
      base::JSONReader::ReadList(json, base::JSON_PARSE_RFC);
  if (!list) {
    return std::nullopt;
  }
  std::vector<CustomFormControlScriptIndex::Entry> entries;
  for (const base::Value& item : *list) {
    const base::DictValue* dict = item.GetIfDict();
    if (!dict) {
      continue;
    }
    const std::string* host = dict->FindString("host");
    const std::string* path = dict->FindString("script_path");
    const std::string* filter = dict->FindString("url_filter");
    // `url_filter` is optional: if it's missing, the script runs on every page
    // of `host`. Skip items whose `url_filter` is present but isn't a string,
    // instead of running their script on every page.
    if (!host || !path || (!filter && dict->contains("url_filter"))) {
      continue;
    }
    // Only read scripts inside `directory`.
    const base::FilePath script_path = base::FilePath::FromUTF8Unsafe(*path);
    if (script_path.empty() || script_path.IsAbsolute() ||
        script_path.ReferencesParent()) {
      continue;
    }
    std::string script;
    if (base::ReadFileToStringWithMaxSize(
            directory.Append(script_path), &script,
            kMaxCustomFormControlScriptSizeBytes)) {
      entries.push_back({.host = *host,
                         .url_filter = filter ? *filter : std::string(),
                         .script = std::move(script)});
    }
  }
  return entries;
}

}  // namespace

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

void CustomFormControlScriptIndex::InitializeFromDirectory(
    const base::FilePath& directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_initialized_ = false;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadEntriesFromDirectory, directory),
      base::BindOnce(
          [](base::WeakPtr<CustomFormControlScriptIndex> self,
             std::optional<std::vector<Entry>> entries) {
            if (!self) {
              return;
            }
            // Keep the current entries if the directory couldn't be read.
            if (entries) {
              self->SetEntries(*std::move(entries));
            }
            self->is_initialized_ = true;
          },
          weak_ptr_factory_.GetWeakPtr()));
}

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
