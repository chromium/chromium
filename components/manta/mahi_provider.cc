// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/mahi_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/channel.h"
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

constexpr char kOauthConsumerName[] = "manta_mahi";
constexpr base::TimeDelta kTimeout = base::Seconds(30);

const net::NetworkTrafficAnnotationTag kMahiTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("help_me_read_request",
                                        R"(
        semantics {
          sender: "Help Me Read"
          description:
            "ChromeOS can help you read articles on webpages or PDF files by "
            "summarizing or answering questions by sending the content, along "
            "with user input question if provided, to Google's servers. Google "
            "returns summary or answers that will be displayed on the screen."
          trigger: "User right clicks within a distillable surface, e.g. a "
                   "webpage or a PDF file opened in the Gallery app with "
                   "enough extractable text content, clicks 'Summarize' button "
                   "or asks a question then clicks 'Send' icon."
          internal {
            contacts {
                email: "cros-manta-team@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: USER_CONTENT
            type: WEB_CONTENT
          }
          data: "The text content of the webpage or the PDF file where the "
                "'Help Me Read' feature is triggered, along with user input "
                "question if provided."
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2024-08-24"
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via 'Help me read' in "
            "ChromeOS's settings under 'System preferences > Search engine > "
            "Use Google AI to get help reading and writing'."
          chrome_policy {
            HelpMeReadSettings {
                HelpMeReadSettings: 2
            }
          }
        })");

constexpr char kUsedHistogram[] = "ChromeOS.Mahi.Used";

// Each feature within Mahi, logged with `kUsedHistogram`.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MahiFeature {
  kSummary = 0,
  kQA = 1,
  kMaxValue = kQA,
};

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
    const ProviderParams& provider_params)
    : BaseProvider(url_loader_factory, identity_manager, provider_params) {}

MahiProvider::MahiProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : BaseProvider(url_loader_factory, identity_manager) {}

MahiProvider::~MahiProvider() = default;

void MahiProvider::Summarize(const std::string& input,
                             const std::string& title,
                             const std::optional<std::string>& url,
                             MantaGenericCallback done_callback) {
  proto::Request request;
  request.set_feature_name(proto::FeatureName::CHROMEOS_READER_SUMMARY);

  auto* input_data = request.add_input_data();
  input_data->set_tag("model_input");
  input_data->set_text(input);

  if (!title.empty()) {
    input_data = request.add_input_data();
    input_data->set_tag("title");
    input_data->set_text(title);
  }

  if (url.has_value() && !url->empty()) {
    input_data = request.add_input_data();
    input_data->set_tag("url");
    input_data->set_text(url.value());
  }

  RequestInternal(
      GURL{GetProviderEndpoint(features::IsMahiUseProdServerEnabled())},
      kOauthConsumerName, kMahiTrafficAnnotationTag, request,
      MantaMetricType::kMahiSummary,
      base::BindOnce(&OnServerResponseOrErrorReceived,
                     std::move(done_callback)),
      kTimeout);

  base::UmaHistogramEnumeration(kUsedHistogram, MahiFeature::kSummary);
}

void MahiProvider::Outline(const std::string& input,
                           const std::string& title,
                           const std::optional<std::string>& url,
                           MantaGenericCallback done_callback) {
  std::move(done_callback)
      .Run(base::Value::Dict(),
           {MantaStatusCode::kGenericError, "Unimplemented"});
}

void MahiProvider::QuestionAndAnswer(const std::string& original_content,
                                     const std::string& title,
                                     const std::optional<std::string>& url,
                                     const std::vector<MahiQAPair> QAHistory,
                                     const std::string& question,
                                     MantaGenericCallback done_callback) {
  proto::Request request;
  request.set_feature_name(proto::FeatureName::CHROMEOS_READER_Q_AND_A);

  auto* input_data = request.add_input_data();
  input_data->set_tag("original_content");
  input_data->set_text(original_content);

  if (!title.empty()) {
    input_data = request.add_input_data();
    input_data->set_tag("title");
    input_data->set_text(title);
  }

  if (url.has_value() && !url->empty()) {
    input_data = request.add_input_data();
    input_data->set_tag("url");
    input_data->set_text(url.value());
  }

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

  RequestInternal(
      GURL{GetProviderEndpoint(features::IsMahiUseProdServerEnabled())},
      kOauthConsumerName, kMahiTrafficAnnotationTag, request,
      MantaMetricType::kMahiQA,
      base::BindOnce(&OnServerResponseOrErrorReceived,
                     std::move(done_callback)),
      kTimeout);

  base::UmaHistogramEnumeration(kUsedHistogram, MahiFeature::kQA);
}

}  // namespace manta
