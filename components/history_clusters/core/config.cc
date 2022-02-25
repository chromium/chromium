// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/config.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace history_clusters {

static Config* s_config = nullptr;

Config::Config() = default;
Config::Config(const Config& other) = default;
Config::~Config() = default;

// Override any parameters that may be provided by Finch.
void OverrideWithFinch(const std::string& application_locale) {
  if (s_config)
    return;

  s_config = new Config;

  if (!base::FeatureList::IsEnabled(internal::kJourneys)) {
    s_config->is_journeys_enabled = false;
  } else {
    // Default to "", because defaulting it to a specific locale makes it hard
    // to allow all locales, since the FeatureParam code interprets an empty
    // string as undefined, and instead returns the default value.
    const base::FeatureParam<std::string> kLocaleOrLanguageAllowlist{
        &internal::kJourneys, "JourneysLocaleOrLanguageAllowlist", ""};

    // Allow comma and colon as delimiters to the language list.
    auto allowlist =
        base::SplitString(kLocaleOrLanguageAllowlist.Get(),
                          ",:", base::WhitespaceHandling::TRIM_WHITESPACE,
                          base::SplitResult::SPLIT_WANT_NONEMPTY);

    // Allow any exact locale matches, and also allow any users where the
    // primary language subtag, e.g. "en" from "en-US" to match any element of
    // the list.
    s_config->is_journeys_enabled =
        allowlist.empty() || base::Contains(allowlist, application_locale) ||
        base::Contains(allowlist, l10n_util::GetLanguage(application_locale));
  }
}

void ResetConfigForTesting() {
  s_config = nullptr;
}

const Config& GetConfig() {
  DCHECK(s_config);

  return *s_config;
}

}  // namespace history_clusters