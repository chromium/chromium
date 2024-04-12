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
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_mahi";

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
    std::string message = std::string();

    // Tries to find more information from filtered_data
    if (manta_response->filtered_data_size() > 0 &&
        manta_response->filtered_data(0).is_output_data()) {
      message = base::StringPrintf(
          "filtered output for: %s",
          proto::FilteredReason_Name(manta_response->filtered_data(0).reason())
              .c_str());
    }
    std::move(callback).Run(base::Value::Dict(),
                            {MantaStatusCode::kBlockedOutputs, message});
    return;
  }

  std::move(callback).Run(
      base::Value::Dict().Set("outputData",
                              std::move(manta_response->output_data(0).text())),
      std::move(manta_status));
}

}  // namespace

MahiProvider::MahiProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    bool is_demo_mode,
    const std::string& chrome_version)
    : BaseProvider(url_loader_factory,
                   identity_manager,
                   is_demo_mode,
                   chrome_version) {}

MahiProvider::MahiProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : MahiProvider(url_loader_factory, identity_manager, false, std::string()) {
}

MahiProvider::~MahiProvider() = default;

void MahiProvider::Summarize(const std::string& input,
                             MantaGenericCallback done_callback) {
  proto::Request request;
  request.set_feature_name(proto::FeatureName::CHROMEOS_READER_SUMMARY);

  auto* input_data = request.add_input_data();
  input_data->set_tag("model_input");
  input_data->set_text(input);

  // TODO(b:333459933): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  RequestInternal(GURL{GetProviderEndpoint(false)}, kOauthConsumerName,
                  MISSING_TRAFFIC_ANNOTATION, request,
                  MantaMetricType::kMahiSummary,
                  base::BindOnce(&OnServerResponseOrErrorReceived,
                                 std::move(done_callback)));
}

void MahiProvider::Outline(const std::string& input,
                           MantaGenericCallback done_callback) {
  std::move(done_callback)
      .Run(base::Value::Dict(),
           {MantaStatusCode::kGenericError, "Unimplemented"});
}

void MahiProvider::QuestionAndAnswer(const std::string& original_content,
                                     const std::vector<MahiQAPair> QAHistory,
                                     const std::string& question,
                                     MantaGenericCallback done_callback) {
  proto::Request request;
  request.set_feature_name(proto::FeatureName::CHROMEOS_READER_Q_AND_A);

  auto* input_data = request.add_input_data();
  input_data->set_tag("original_content");
  input_data->set_text(original_content);

  input_data = request.add_input_data();
  input_data->set_tag("new_question");
  input_data->set_text(question);

  for (const auto& [previous_question, previous_answer] : QAHistory) {
    input_data = request.add_input_data();
    input_data->set_tag("previous_question");
    input_data->set_text(previous_question);

    input_data = request.add_input_data();
    input_data->set_tag("previous_answer");
    input_data->set_text(previous_answer);
  }

  // TODO(b:288019728): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  RequestInternal(GURL{GetProviderEndpoint(false)}, kOauthConsumerName,
                  MISSING_TRAFFIC_ANNOTATION, request, MantaMetricType::kMahiQA,
                  base::BindOnce(&OnServerResponseOrErrorReceived,
                                 std::move(done_callback)));
}

}  // namespace manta
