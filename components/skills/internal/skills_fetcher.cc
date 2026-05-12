// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_fetcher.h"

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/skills/features.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skills_metrics.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace skills {

namespace {

constexpr net::NetworkTrafficAnnotationTag kSkillsFetcherNetworkTag =
    net::DefineNetworkTrafficAnnotation("skills_fetcher", R"(
      semantics {
        sender: "Skills Service"
        description:
          "Downloads a list of first-party agent prompts from the Skills API. "
          "These skills are displayed on chrome://skills, allowing users to "
          "browse a list of first-party skills."
        trigger:
          "Triggered on chrome://skills page load or attempt "
          "to save a skill."
        data: "OAuth2 access token."
        destination: GOOGLE_OWNED_SERVICE
        user_data {
            type: ACCESS_TOKEN
        }
        internal {
            contacts {
              email: "chrome-skills-eng-team@google.com"
            }
        }
        last_reviewed: "2026-05-08"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature can be disabled by turning off the Skills "
          "feature in chrome://flags/#skills."
        chrome_policy {
          GeminiSettings {
            GeminiSettings: 1
          }
        }
      })");

}  // namespace

SkillsFetcher::SkillsFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager) {}

SkillsFetcher::~SkillsFetcher() = default;

void SkillsFetcher::FetchDiscoverySkills(OnFetchCompleteCallback callback) {
  if (endpoint_fetcher_) {
    // Request already in progress.
    return;
  }

  GURL url(features::kSkillsServiceApiUrl.Get());

  endpoint_fetcher_ = std::make_unique<endpoint_fetcher::EndpointFetcher>(
      url_loader_factory_, identity_manager_,
      endpoint_fetcher::EndpointFetcher::RequestParams::Builder(
          endpoint_fetcher::HttpMethod::kGet, kSkillsFetcherNetworkTag)
          .SetUrl(url)
          .SetAuthType(endpoint_fetcher::AuthType::OAUTH)
          .SetOAuthConsumerId(signin::OAuthConsumerId::kSkillsService)
          .SetConsentLevel(signin::ConsentLevel::kSignin)
          .Build());

  endpoint_fetcher_->Fetch(base::BindOnce(&SkillsFetcher::OnResponseFetched,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          std::move(callback)));
}

void SkillsFetcher::OnResponseFetched(
    OnFetchCompleteCallback callback,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  endpoint_fetcher_.reset();

  if (!response || response->http_status_code != net::HTTP_OK) {
    RecordSkillsFetchResult(SkillsFetchResult::kNetworkError);
    std::move(callback).Run(nullptr);
    return;
  }

  if (response->response.empty()) {
    RecordSkillsFetchResult(SkillsFetchResult::kEmptyResponseBody);
    std::move(callback).Run(nullptr);
    return;
  }

  skills::proto::SkillsList skills_list;
  if (!skills_list.ParseFromString(response->response)) {
    RecordSkillsFetchResult(SkillsFetchResult::kProtoParseFailure);
    std::move(callback).Run(nullptr);
    return;
  }

  auto first_party_skill_data = std::make_unique<FirstPartySkillData>();

  // If a skill curated by field is not set, default to Chrome
  for (auto& skill : *skills_list.mutable_skills()) {
    if (!skill.has_curated_by() || skill.curated_by().empty()) {
      skill.set_curated_by("Chrome");
    }
  }

  first_party_skill_data->skills_list.insert(
      first_party_skill_data->skills_list.end(),
      std::make_move_iterator(skills_list.mutable_skills()->begin()),
      std::make_move_iterator(skills_list.mutable_skills()->end()));

  first_party_skill_data->topics_info_list.insert(
      first_party_skill_data->topics_info_list.end(),
      std::make_move_iterator(skills_list.mutable_topics_info_list()->begin()),
      std::make_move_iterator(skills_list.mutable_topics_info_list()->end()));

  RecordSkillsFetchResult(SkillsFetchResult::kSuccess);
  std::move(callback).Run(std::move(first_party_skill_data));
}

}  // namespace skills
