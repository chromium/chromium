// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_features.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace password_manager {

namespace {

using ActorByOriginMap = base::flat_map<url::Origin, RemoteActorType>;

constexpr char kAllowedHostsJsonKey[] = "allowed_hosts";

static_assert(kAllRemoteActorTypes.size() ==
              static_cast<size_t>(RemoteActorType::kMaxValue) + 1);

std::optional<RemoteActorType> FindActorTypeForOrigin(
    const ActorByOriginMap& actor_by_origin,
    const url::Origin& origin) {
  auto it = actor_by_origin.find(origin);
  return it != actor_by_origin.end() ? std::optional(it->second) : std::nullopt;
}

bool IsActorTypeEnabled(RemoteActorType actor_type) {
  switch (actor_type) {
    case RemoteActorType::kGemini:
      return features::RemoteActorCredentialSharingGeminiEnabled();
    case RemoteActorType::kSearch:
      return features::RemoteActorCredentialSharingSearchEnabled();
  }
  NOTREACHED();
}

// Parses `entry` (either a full HTTPS URL like "https://www.google.com" or a
// bare hostname like "www.google.com") into an HTTPS `url::Origin`. Rejects
// non-HTTPS schemes, opaque origins, leading/extra slashes, user/password
// credentials, paths, queries, and fragments.
std::optional<url::Origin> ParseHttpsOriginOrHost(std::string_view entry) {
  std::string_view trimmed = base::TrimWhitespaceASCII(entry, base::TRIM_ALL);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  std::string normalized_input =
      base::StartsWith(trimmed, "https://",
                       base::CompareCase::INSENSITIVE_ASCII)
          ? std::string(trimmed)
          : base::StrCat(
                {url::kHttpsScheme, url::kStandardSchemeSeparator, trimmed});
  GURL candidate_url(normalized_input);
  if (!candidate_url.is_valid() || !candidate_url.SchemeIs(url::kHttpsScheme)) {
    return std::nullopt;
  }

  url::Origin candidate_origin = url::Origin::Create(candidate_url);
  if (candidate_origin.opaque() || candidate_url != candidate_origin.GetURL() ||
      !base::EqualsCaseInsensitiveASCII(normalized_input,
                                        candidate_origin.Serialize())) {
    return std::nullopt;
  }
  return candidate_origin;
}

void AddConfiguredAllowedOriginsForActor(RemoteActorType actor_type,
                                         std::string_view raw_json,
                                         ActorByOriginMap& actor_by_origin) {
  if (raw_json.empty()) {
    return;
  }
  std::optional<base::DictValue> actor_dict =
      base::JSONReader::ReadDict(raw_json, base::JSON_PARSE_RFC);
  if (!actor_dict) {
    return;
  }
  const base::ListValue* allowed_hosts =
      actor_dict->FindList(kAllowedHostsJsonKey);
  if (!allowed_hosts) {
    return;
  }

  for (const base::Value& entry : *allowed_hosts) {
    if (!entry.is_string()) {
      continue;
    }
    if (std::optional<url::Origin> candidate_origin =
            ParseHttpsOriginOrHost(entry.GetString())) {
      actor_by_origin.try_emplace(std::move(*candidate_origin), actor_type);
    }
  }
}

ActorByOriginMap ParseConfiguredAllowedOrigins(
    std::string_view gemini_raw_json,
    std::string_view search_raw_json) {
  ActorByOriginMap actor_by_origin;
  AddConfiguredAllowedOriginsForActor(RemoteActorType::kGemini, gemini_raw_json,
                                      actor_by_origin);
  AddConfiguredAllowedOriginsForActor(RemoteActorType::kSearch, search_raw_json,
                                      actor_by_origin);
  return actor_by_origin;
}

std::optional<RemoteActorType> GetConfiguredActorTypeForOrigin(
    const url::Origin& origin) {
  static base::NoDestructor<base::Lock> cache_lock;
  static base::NoDestructor<std::string> cached_gemini_raw_json;
  static base::NoDestructor<std::string> cached_search_raw_json;
  static base::NoDestructor<ActorByOriginMap> cached_actor_by_origin;

  std::string gemini_raw_json =
      GetRemoteActorConfigJson(RemoteActorType::kGemini);
  std::string search_raw_json =
      GetRemoteActorConfigJson(RemoteActorType::kSearch);
  if (gemini_raw_json.empty() && search_raw_json.empty()) {
    return std::nullopt;
  }

  base::AutoLock auto_lock(*cache_lock);
  if (*cached_gemini_raw_json != gemini_raw_json ||
      *cached_search_raw_json != search_raw_json) {
    *cached_actor_by_origin =
        ParseConfiguredAllowedOrigins(gemini_raw_json, search_raw_json);
    *cached_gemini_raw_json = std::move(gemini_raw_json);
    *cached_search_raw_json = std::move(search_raw_json);
  }

  return FindActorTypeForOrigin(*cached_actor_by_origin, origin);
}

}  // namespace

std::string GetRemoteActorConfigJson(RemoteActorType actor_type) {
  switch (actor_type) {
    case RemoteActorType::kGemini:
      return features::kRemoteActorGeminiConfigJson.Get();
    case RemoteActorType::kSearch:
      return features::kRemoteActorSearchConfigJson.Get();
  }
  NOTREACHED();
}

std::optional<RemoteActorType> GetRemoteActorTypeForOrigin(
    const url::Origin& origin) {
  if (!features::RemoteActorCredentialSharingEnabled() ||
      origin.scheme() != url::kHttpsScheme) {
    return std::nullopt;
  }

  std::optional<RemoteActorType> actor_type =
      GetConfiguredActorTypeForOrigin(origin);
  if (!actor_type.has_value()) {
    static const base::NoDestructor<ActorByOriginMap> kDefaultActorByOrigin({
        {url::Origin::Create(GURL("https://gemini.google.com")),
         RemoteActorType::kGemini},
        {url::Origin::Create(GURL("https://gemini.corp.google.com")),
         RemoteActorType::kGemini},
        {url::Origin::Create(GURL("https://gemini-preprod.corp.google.com")),
         RemoteActorType::kGemini},
        {url::Origin::Create(GURL("https://gemini-staging.corp.google.com")),
         RemoteActorType::kGemini},
        {url::Origin::Create(GURL("https://gemini-autopush.corp.google.com")),
         RemoteActorType::kGemini},
        {url::Origin::Create(GURL("https://search.google.com")),
         RemoteActorType::kSearch},
    });

    actor_type = FindActorTypeForOrigin(*kDefaultActorByOrigin, origin);
  }

  if (!actor_type.has_value() || !IsActorTypeEnabled(*actor_type)) {
    return std::nullopt;
  }
  return actor_type;
}

bool IsRemoteActorCredentialSharingAllowedForOrigin(const url::Origin& origin) {
  return GetRemoteActorTypeForOrigin(origin).has_value();
}

}  // namespace password_manager
