// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_deduplication/url_deduplication_helper.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/url_deduplication/deduplication_strategy.h"
#include "components/url_deduplication/url_strip_handler.h"
#include "url/gurl.h"

namespace url_deduplication {

URLDeduplicationHelper::URLDeduplicationHelper(
    std::vector<std::unique_ptr<URLStripHandler>> strip_handlers,
    DeduplicationStrategy strategy)
    : strip_handlers_(std::move(strip_handlers)), strategy_(strategy) {}

URLDeduplicationHelper::URLDeduplicationHelper(DeduplicationStrategy strategy)
    : strategy_(strategy) {}

URLDeduplicationHelper::~URLDeduplicationHelper() = default;

std::string URLDeduplicationHelper::ComputeURLDeduplicationKey(
    const GURL& url,
    const std::string& title) {
  GURL stripped_destination_url = url;
  for (auto& handler : strip_handlers_) {
    GURL temp_url = handler->StripExtraParams(url);
    if (temp_url.is_valid() && !temp_url.is_empty()) {
      stripped_destination_url = temp_url;
    }
    if (stripped_destination_url != url) {
      return stripped_destination_url.spec();
    }
  }

  // |replacements| keeps all the substitutions we're going to make to
  // from {destination_url} to {stripped_destination_url}.  |need_replacement|
  // is a helper variable that helps us keep track of whether we need
  // to apply the replacement.
  bool needs_replacement = false;
  GURL::Replacements replacements;

  // Strip various common prefixes in order to group the resulting hostnames
  // together and avoid duplicates.
  std::string host = stripped_destination_url.host();
  for (std::string_view prefix : strategy_.excluded_prefixes) {
    if (host.size() > prefix.size() &&
        base::StartsWith(host, prefix, base::CompareCase::INSENSITIVE_ASCII)) {
      replacements.SetHostStr(std::string_view(host).substr(prefix.size()));
      needs_replacement = true;
      break;
    }
  }

  // Replace https protocol with http, as long as the user didn't explicitly
  // specify one of the two.
  if (stripped_destination_url.SchemeIs(url::kHttpsScheme) &&
      strategy_.update_scheme) {
    replacements.SetSchemeStr(url::kHttpScheme);
    needs_replacement = true;
  }

  if (strategy_.clear_ref) {
    replacements.ClearRef();
    needs_replacement = true;
  }

  if (strategy_.clear_path) {
    replacements.ClearPath();
    needs_replacement = true;
  }

  if (strategy_.clear_query) {
    replacements.ClearQuery();
    needs_replacement = true;
  }

  if (strategy_.clear_username) {
    replacements.ClearUsername();
    needs_replacement = true;
  }

  if (strategy_.clear_password) {
    replacements.ClearPassword();
    needs_replacement = true;
  }

  if (strategy_.clear_port) {
    replacements.ClearPort();
    needs_replacement = true;
  }

  if (needs_replacement) {
    stripped_destination_url =
        stripped_destination_url.ReplaceComponents(replacements);
  }

  if (strategy_.include_title) {
    return base::StrCat({stripped_destination_url.spec(), "#", title});
  }

  return stripped_destination_url.spec();
}

}  // namespace url_deduplication
