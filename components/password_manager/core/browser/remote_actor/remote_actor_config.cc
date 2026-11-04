// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/remote_actor/remote_actor_config.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace password_manager {

namespace {

// Task ID namespace for Gemini Web (Spark) conversation IDs in APS.
constexpr int kGeminiTaskNamespaceId = 30;
// Task ID namespace for Google Search Agent task IDs in APS.
constexpr int kSearchTaskNamespaceId = 41;

struct ActorDefaultSpec {
  int display_name_message_id;
  std::string_view learn_more_url;
  std::string_view oauth_client_id;
  int task_namespace_id;
};

ActorDefaultSpec GetActorDefaultSpec(RemoteActorType actor_type) {
  switch (actor_type) {
    case RemoteActorType::kGemini:
      return {
          .display_name_message_id = IDS_REMOTE_ACTOR_GEMINI_NAME,
          .learn_more_url = "https://support.google.com/gemini?p=lm_gpm_data",
          .oauth_client_id = features::kRemoteActorOAuthClientId.default_value,
          .task_namespace_id = kGeminiTaskNamespaceId,
      };
    case RemoteActorType::kSearch:
      return {
          .display_name_message_id = IDS_REMOTE_ACTOR_SEARCH_AGENT_NAME,
          .learn_more_url = "https://g.co/privacypolicy",
          .oauth_client_id =
              "240392999166-p989rucds3hg2lidtif5huititvl4ln1."
              "apps.googleusercontent.com",
          .task_namespace_id = kSearchTaskNamespaceId,
      };
  }
  NOTREACHED();
}

GURL ResolveHttpsUrl(const std::string* candidate_spec) {
  if (candidate_spec && !candidate_spec->empty()) {
    GURL candidate(*candidate_spec);
    if (candidate.is_valid() && candidate.SchemeIs(url::kHttpsScheme)) {
      return candidate;
    }
  }
  return GURL();
}

}  // namespace

RemoteActorConfig GetRemoteActorConfig(RemoteActorType actor_type) {
  const ActorDefaultSpec defaults = GetActorDefaultSpec(actor_type);

  RemoteActorConfig config;
  config.actor_type = actor_type;
  config.display_name =
      l10n_util::GetStringUTF16(defaults.display_name_message_id);
  config.learn_more_url = GURL(defaults.learn_more_url);
  config.oauth_client_id = defaults.oauth_client_id;
  config.task_namespace_id = defaults.task_namespace_id;

  if (actor_type == RemoteActorType::kGemini) {
    if (std::string testing_actor_name =
            features::kRemoteActorCredentialSharingActorNameForTesting.Get();
        !testing_actor_name.empty() &&
        testing_actor_name !=
            features::kRemoteActorCredentialSharingActorNameForTesting
                .default_value) {
      config.display_name = base::UTF8ToUTF16(testing_actor_name);
    }
    if (std::string legacy_oauth_client_id =
            features::kRemoteActorOAuthClientId.Get();
        !legacy_oauth_client_id.empty()) {
      config.oauth_client_id = std::move(legacy_oauth_client_id);
    }
  }

  std::string raw_json = features::kRemoteActorConfigsJson.Get();
  if (raw_json.empty()) {
    return config;
  }
  std::optional<base::DictValue> root_dict =
      base::JSONReader::ReadDict(raw_json, base::JSON_PARSE_RFC);
  const base::DictValue* actor_dict =
      root_dict ? root_dict->FindDict(GetRemoteActorKey(actor_type)) : nullptr;
  if (!actor_dict) {
    return config;
  }

  if (const std::string* actor_name = actor_dict->FindString("actor_name");
      actor_name && !actor_name->empty()) {
    config.display_name = base::UTF8ToUTF16(*actor_name);
  }
  if (GURL learn_more_url =
          ResolveHttpsUrl(actor_dict->FindString("learn_more_url"));
      learn_more_url.is_valid()) {
    config.learn_more_url = std::move(learn_more_url);
  }
  if (GURL icon_url = ResolveHttpsUrl(actor_dict->FindString("icon_url"));
      icon_url.is_valid()) {
    config.icon_url = std::move(icon_url);
  }
  if (const std::string* oauth_client_id =
          actor_dict->FindString("oauth_client_id");
      oauth_client_id && !oauth_client_id->empty()) {
    config.oauth_client_id = *oauth_client_id;
  }
  if (std::optional<int> namespace_id =
          actor_dict->FindInt("task_namespace_id");
      namespace_id.has_value() && *namespace_id > 0) {
    config.task_namespace_id = *namespace_id;
  }

  return config;
}

std::optional<RemoteActorConfig> GetRemoteActorConfigForOrigin(
    const url::Origin& origin) {
  std::optional<RemoteActorType> actor_type =
      GetRemoteActorTypeForOrigin(origin);
  if (!actor_type.has_value()) {
    return std::nullopt;
  }
  return GetRemoteActorConfig(*actor_type);
}

}  // namespace password_manager
