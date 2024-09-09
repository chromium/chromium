// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/orca_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/features.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace manta {

namespace {

constexpr char kOauthConsumerName[] = "manta_orca";
constexpr base::TimeDelta kTimeout = base::Seconds(30);

using Tone = proto::RequestConfig::Tone;

std::optional<Tone> GetTone(const std::string& tone) {
  static constexpr auto tone_map =
      base::MakeFixedFlatMap<std::string_view, Tone>({
          {"UNSPECIFIED", proto::RequestConfig::UNSPECIFIED},
          {"SHORTEN", proto::RequestConfig::SHORTEN},
          {"ELABORATE", proto::RequestConfig::ELABORATE},
          {"REPHRASE", proto::RequestConfig::REPHRASE},
          {"FORMALIZE", proto::RequestConfig::FORMALIZE},
          {"EMOJIFY", proto::RequestConfig::EMOJIFY},
          {"FREEFORM_REWRITE", proto::RequestConfig::FREEFORM_REWRITE},
          {"FREEFORM_WRITE", proto::RequestConfig::FREEFORM_WRITE},
          {"PROOFREAD", proto::RequestConfig::PROOFREAD},

      });
  const auto iter = tone_map.find(tone);

  return iter != tone_map.end() ? std::optional<Tone>(iter->second)
                                : std::nullopt;
}

std::optional<proto::Request> ComposeRequest(
    const std::map<std::string, std::string>& input) {
  const auto& tone_iter = input.find("tone");
  if (tone_iter == input.end()) {
    DVLOG(1) << "Tone not found in the parameters";
    return std::nullopt;
  }

  auto tone = GetTone(tone_iter->second);
  if (tone == std::nullopt) {
    DVLOG(1) << "Invalid tone";
    return std::nullopt;
  }

  proto::Request request;
  request.set_feature_name(proto::FeatureName::TEXT_TEST);
  auto& request_config = *request.mutable_request_config();
  request_config.set_tone(tone.value());

  for (const auto& kv : input) {
    auto* input_data = request.add_input_data();
    input_data->set_tag(kv.first);
    input_data->set_text(kv.second);
  }

  return request;
}

void OnServerResponseOrErrorReceived(
    MantaGenericCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  if (manta_status.status_code != MantaStatusCode::kOk) {
    DCHECK(manta_response == nullptr);
    std::move(callback).Run(base::Value::Dict(), std::move(manta_status));
    return;
  }

  DCHECK(manta_response != nullptr);

  auto output_data_list = base::Value::List();
  for (const auto& output_data : manta_response->output_data()) {
    if (output_data.has_text()) {
      output_data_list.Append(
          base::Value::Dict().Set("text", output_data.text()));
    }
  }

  if (output_data_list.size() == 0) {
    std::move(callback).Run(
        base::Value::Dict(),
        {MantaStatusCode::kBlockedOutputs, /*message=*/std::string()});
    return;
  }

  std::move(callback).Run(
      base::Value::Dict().Set("outputData", std::move(output_data_list)),
      std::move(manta_status));
}

}  // namespace

OrcaProvider::OrcaProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const ProviderParams& provider_params)
    : BaseProvider(url_loader_factory, identity_manager, provider_params) {}

OrcaProvider::~OrcaProvider() = default;

void OrcaProvider::Call(const std::map<std::string, std::string>& input,
                        MantaGenericCallback done_callback) {
  std::optional<proto::Request> request = ComposeRequest(input);
  if (request == std::nullopt) {
    std::move(done_callback)
        .Run(base::Value::Dict(),
             {MantaStatusCode::kInvalidInput, /*message=*/std::string()});
    return;
  }

  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("help_me_write_request", R"(
        semantics {
          sender: "Help Me Write"
          description:
            "ChromeOS can help you write and rewrite text by sending a "
            "freeform text query, along with any selected text, to Google's "
            "servers. Google returns suggested text which you may choose to "
            "insert into the selected text field."
          trigger: "User right clicks within an editable text field and "
                   "chooses 'Help me write' and then chooses a preset query or "
                   "enters a free-form text query."
          internal {
            contacts {
                email: "cros-manta-team@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: USER_CONTENT
          }
          data: "A preset or free-form user query, along with any text a user "
                "has selected in the editable text field. Query metadata is "
                "also sent including the user's preferred input language."
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-03-15"
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via 'Help me write' in "
            "ChromeOS's settings under 'Inputs > Suggestions'."
          chrome_policy {
            OrcaEnabled {
                OrcaEnabled: false
            }
          }
        })");

  RequestInternal(
      GURL{GetProviderEndpoint(features::IsOrcaUseProdServerEnabled())},
      kOauthConsumerName, traffic_annotation, request.value(),
      MantaMetricType::kOrca,
      base::BindOnce(&OnServerResponseOrErrorReceived,
                     std::move(done_callback)),
      kTimeout);
}

}  // namespace manta
