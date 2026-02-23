// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_downloader.h"

#include "base/functional/bind.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skills_metrics.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace skills {

namespace {
constexpr size_t kMaxDownloadBytes = 1024 * 1024;  // 1MB
// Max number of retries for fetching the configuration.
constexpr int kMaxRetries = 3;
// The conditions for which a retry will trigger.
constexpr int kRetryMode = network::SimpleURLLoader::RETRY_ON_5XX |
                           network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                           network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED;

constexpr net::NetworkTrafficAnnotationTag kSkillsDownloaderNetworkTag =
    net::DefineNetworkTrafficAnnotation("skills_service_loader", R"(
      semantics {
        sender: "Skills Service"
        description:
          "Downloads a list of first-party agent prompts, also called skills, "
          "that are accessible to users via chrome://skills/discover-skills."
        trigger:
          "Triggered on chrome://skills/discover-skills page load or attempt "
          "to save a skill."
        data: "None"
        destination: GOOGLE_OWNED_SERVICE
        user_data {
            type: NONE
        }
        internal {
            contacts {
              email: "//components/skills/OWNERS"
            }
        }
        last_reviewed: "2026-01-27"
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled in settings."
        policy_exception_justification: "World-readable data is being "
        "downloaded to be displayed on all Chrome accounts."
      })");

std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(
    const GURL& url,
    const std::string_view last_modified_header) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = net::HttpRequestHeaders::kGetMethod;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kIfModifiedSince,
                                      last_modified_header);
  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kSkillsDownloaderNetworkTag);
}
}  // namespace

SkillsDownloader::SkillsDownloader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

SkillsDownloader::~SkillsDownloader() = default;

void SkillsDownloader::FetchDiscoverySkills(OnFetchCompleteCallback callback) {
  auto wrapped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), nullptr);

  // If there is already a request in progress, don't try again.
  if (!url_loader_factory_ || pending_request_) {
    return;
  }

  GURL destination = GURL(kSkillsDownloaderGstaticUrl);
  pending_request_ = CreateSimpleURLLoader(destination, last_modified_header_);
  pending_request_->SetRetryOptions(kMaxRetries, kRetryMode);
  pending_request_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&SkillsDownloader::OnUrlDownloadComplete,
                     self_ptr_factory_.GetWeakPtr(),
                     std::move(wrapped_callback)),
      kMaxDownloadBytes);
}

void SkillsDownloader::OnUrlDownloadComplete(
    OnFetchCompleteCallback callback,
    std::optional<std::string> response_body) {
  // Move the pending request here so it's deleted when this function ends.
  std::unique_ptr<network::SimpleURLLoader> request =
      std::move(pending_request_);

  if (!request) {
    return;
  }

  if (!request->ResponseInfo() || !request->ResponseInfo()->headers) {
    RecordSkillsFetchResult(SkillsFetchResult::kEmptyResponseHeader);
    return;
  }

  int response_code = request->ResponseInfo()->headers->response_code();
  RecordSkillsHttpCode(response_code);
  if (response_code != net::HTTP_OK) {
    RecordSkillsFetchResult(SkillsFetchResult::kNetworkError);
    return;
  }

  if (!response_body.has_value() || response_body->empty()) {
    RecordSkillsFetchResult(SkillsFetchResult::kEmptyResponseBody);
    return;
  }

  auto skills_list = std::make_unique<skills::proto::SkillsList>();
  if (!skills_list->ParseFromString(response_body.value())) {
    RecordSkillsFetchResult(SkillsFetchResult::kProtoParseFailure);
    return;
  }

  RecordSkillsFetchResult(SkillsFetchResult::kSuccess);

  std::string last_modified_value;
  if (request->ResponseInfo()->headers->EnumerateHeader(
          nullptr, "Last-Modified", &last_modified_value)) {
    last_modified_header_ = last_modified_value;
  }

  auto skills_map = std::make_unique<SkillsMap>();
  for (auto& skill : *skills_list->mutable_skills()) {
    skills_map->insert_or_assign(skill.id(), std::move(skill));
  }

  std::move(callback).Run(std::move(skills_map));
}

}  // namespace skills
