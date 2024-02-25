// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/mahi_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace manta {

namespace {

// TODO(b:318566801): use Orca free form for dogfood.
constexpr char kOauthConsumerName[] = "manta_orca";
constexpr char kHttpMethod[] = "POST";
constexpr char kHttpContentType[] = "application/x-protobuf";
constexpr char kProdEndpointUrl[] = "https://aratea-pa.googleapis.com/generate";
constexpr char kOAuthScope[] = "https://www.googleapis.com/auth/mdi.aratea";
constexpr base::TimeDelta kTimeout = base::Seconds(30);

constexpr char kSummaryTemplate[] =
    "Summarize the following passage into a single 2-4 sentence paragraph: %s";
constexpr char kOutlineTemplate[] =
    "Create a thematic outline organized by topics based on the following "
    "text: %s";

void OnServerResponseOrErrorReceived(
    MantaGenericCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  if (manta_status.status_code != MantaStatusCode::kOk) {
    CHECK(manta_response == nullptr);
    std::move(callback).Run(base::Value::Dict(), std::move(manta_status));
    return;
  }

  CHECK(manta_response != nullptr);

  if (manta_response->output_data_size() < 1 ||
      !manta_response->output_data(0).has_text()) {
    std::move(callback).Run(base::Value::Dict(),
                            {MantaStatusCode::kBlockedOutputs, std::string()});
    return;
  }

  std::move(callback).Run(
      base::Value::Dict().Set("outputData",
                              std::move(manta_response->output_data(0).text())),
      std::move(manta_status));
}

proto::Request ComposeRequest(const std::string& input) {
  proto::Request request;
  request.set_feature_name(proto::FeatureName::TEXT_TEST);

  auto& request_config = *request.mutable_request_config();
  request_config.set_tone(proto::RequestConfig::FREEFORM_WRITE);

  auto* input_data = request.add_input_data();
  input_data->set_tag("freeform_user_prompt");
  input_data->set_text(input);

  return request;
}

}  // namespace

MahiProvider::MahiProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(url_loader_factory) {
  CHECK(identity_manager);
  identity_manager_observation_.Observe(identity_manager);
}

MahiProvider::~MahiProvider() = default;

void MahiProvider::Summarize(const std::string& input,
                             MantaGenericCallback done_callback) {
  RequestInternal(base::StringPrintf(kSummaryTemplate, input.c_str()),
                  std::move(done_callback));
}

void MahiProvider::Outline(const std::string& input,
                           MantaGenericCallback done_callback) {
  RequestInternal(base::StringPrintf(kOutlineTemplate, input.c_str()),
                  std::move(done_callback));
}

void MahiProvider::RequestInternal(const std::string& input,
                                   MantaGenericCallback done_callback) {
  if (!identity_manager_observation_.IsObserving()) {
    std::move(done_callback)
        .Run(base::Value::Dict(), {MantaStatusCode::kNoIdentityManager});
    return;
  }

  proto::Request request = ComposeRequest(input);

  std::string serialized_request;
  request.SerializeToString(&serialized_request);

  std::unique_ptr<EndpointFetcher> fetcher = CreateEndpointFetcher(
      GURL{kProdEndpointUrl}, {kOAuthScope}, serialized_request);

  EndpointFetcher* const fetcher_ptr = fetcher.get();
  MantaProtoResponseCallback internal_callback = base::BindOnce(
      &OnServerResponseOrErrorReceived, std::move(done_callback));
  fetcher_ptr->Fetch(base::BindOnce(&OnEndpointFetcherComplete,
                                    std::move(internal_callback),
                                    std::move(fetcher)));
}

void MahiProvider::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (identity_manager_observation_.IsObservingSource(identity_manager)) {
    identity_manager_observation_.Reset();
  }
}

std::unique_ptr<EndpointFetcher> MahiProvider::CreateEndpointFetcher(
    const GURL& url,
    const std::vector<std::string>& scopes,
    const std::string& post_data) {
  CHECK(identity_manager_observation_.IsObserving());
  // TODO(b:288019728): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*oauth_consumer_name=*/kOauthConsumerName,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*scopes=*/scopes,
      /*timeout=*/kTimeout,
      /*post_data=*/post_data,
      /*annotation_tag=*/MISSING_TRAFFIC_ANNOTATION,
      /*identity_manager=*/identity_manager_observation_.GetSource(),
      /*consent_level=*/signin::ConsentLevel::kSignin);
}

}  // namespace manta
