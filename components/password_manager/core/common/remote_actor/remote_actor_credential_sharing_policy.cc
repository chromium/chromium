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

constexpr char kGeminiKey[] = "gemini";
constexpr char kSearchKey[] = "search";

constexpr char kAllowedHostsJsonKey[] = "allowed_hosts";

static_assert(kAllRemoteActorTypes.size() ==
              static_cast<size_t>(RemoteActorType::kMaxValue) + 1);

std::optional<RemoteActorType> FindActorTypeForOrigin(
    const ActorByOriginMap& actor_by_origin,
    const url::Origin& origin) {
  auto it = actor_by_origin.find(origin);
  return it != actor_by_origin.end() ? std::optional(it->second) : std::nullopt;
}

// Parses `entry` (either a full HTTPS URL like "https://www.google.com" or a
// bare hostname like "www.google.com") into an HTTPS `url::Origin`. Rejects
// non-HTTPS schemes, opaque origins, leading/extra slashes, user/password
// credentials, paths, queries, and fragments.
std::optional<url::Origin> ParseHttpsOriginOrHost(std::string_view entry) {
  std::string_view trimmed =
      base::TrimWhitespaceASCII(entry, base::TRIM_ALL);
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

ActorByOriginMap ParseConfiguredAllowedOrigins(std::string_view raw_json) {
  ActorByOriginMap actor_by_origin;
  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(raw_json, base::JSON_PARSE_RFC);
  if (!parsed) {
    return actor_by_origin;
  }

  for (RemoteActorType actor_type : kAllRemoteActorTypes) {
    const base::DictValue* actor_dict =
        parsed->FindDict(GetRemoteActorKey(actor_type));
    if (!actor_dict) {
      continue;
    }
    const base::ListValue* allowed_hosts =
        actor_dict->FindList(kAllowedHostsJsonKey);
    if (!allowed_hosts) {
      continue;
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

  return actor_by_origin;
}

std::optional<RemoteActorType> GetConfiguredActorTypeForOrigin(
    const url::Origin& origin) {
  static base::NoDestructor<base::Lock> cache_lock;
  static base::NoDestructor<std::string> cached_raw_json;
  static base::NoDestructor<ActorByOriginMap> cached_actor_by_origin;

  std::string raw_json = features::kRemoteActorConfigsJson.Get();
  if (raw_json.empty()) {
    return std::nullopt;
  }

  base::AutoLock auto_lock(*cache_lock);
  if (*cached_raw_json != raw_json) {
    *cached_actor_by_origin = ParseConfiguredAllowedOrigins(raw_json);
    *cached_raw_json = std::move(raw_json);
  }

  return FindActorTypeForOrigin(*cached_actor_by_origin, origin);
}

}  // namespace

std::string_view GetRemoteActorKey(RemoteActorType actor_type) {
  switch (actor_type) {
    case RemoteActorType::kGemini:
      return kGeminiKey;
    case RemoteActorType::kSearch:
      return kSearchKey;
  }
  NOTREACHED();
}

std::optional<RemoteActorType> GetRemoteActorTypeForOrigin(
    const url::Origin& origin) {
  if (!features::RemoteActorCredentialSharingEnabled() ||
      origin.scheme() != url::kHttpsScheme) {
    return std::nullopt;
  }

  std::string allowed_host =
      features::kRemoteActorCredentialSharingAllowedHostForTesting.Get();
  if (!allowed_host.empty() && origin.host() == allowed_host) {
    // Note: For the feature flag configured allowed host parameter, we only
    // verify the scheme and host, skipping the port check. This is necessary
    // because in browser tests, the EmbeddedTestServer runs on a dynamically
    // allocated random port.
    std::string allowed_host_actor =
        features::kRemoteActorCredentialSharingAllowedHostActorForTesting.Get();
    for (RemoteActorType actor_type : kAllRemoteActorTypes) {
      if (base::EqualsCaseInsensitiveASCII(allowed_host_actor,
                                           GetRemoteActorKey(actor_type))) {
        return actor_type;
      }
    }
    return RemoteActorType::kGemini;
  }

  if (std::optional<RemoteActorType> configured_actor_type =
          GetConfiguredActorTypeForOrigin(origin)) {
    return configured_actor_type;
  }

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

  return FindActorTypeForOrigin(*kDefaultActorByOrigin, origin);
}

bool IsRemoteActorCredentialSharingAllowedForOrigin(const url::Origin& origin) {
  return GetRemoteActorTypeForOrigin(origin).has_value();
}

}  // namespace password_manager
