// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/scanner_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/features.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/common.pb.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace manta {

namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(30);
constexpr auto kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chromeos_scanner_provider", R"(
      semantics {
        sender: "ChromeOS Scanner"
        description:
          "Requests extracted details for a selected region of their screen "
          "from the Scanner service."
        trigger:
          "User selecting a region via the Scanner UI."
        internal {
          contacts {
            email: "e14s-eng@google.com"
          }
        }
        user_data {
          type: USER_CONTENT
        }
        data:
          "A selected region of their screen."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2025-02-25"
      }
      policy {
        cookies_allowed: NO
        setting:
          "No setting. Users must take explicit action to trigger the feature."
        policy_exception_justification:
          "Not implemented, not considered useful. This request is part of a "
          "flow which is user-initiated."
      }
    )");

bool IsValidScannerInput(const proto::ScannerInput& scanner_input) {
  return scanner_input.image().size() > 0;
}

std::unique_ptr<proto::ScannerOutput> GetScannerOutput(
    const proto::Response& manta_response) {
  // There should only be one output data.
  if (manta_response.output_data_size() != 1) {
    return nullptr;
  }
  const proto::OutputData& output_data = manta_response.output_data(0);

  // There should be a custom proto.
  if (!output_data.has_custom()) {
    return nullptr;
  }

  // The custom proto should be a ScannerOutput.
  if (output_data.custom().type_url() != kScannerOutputTypeUrl) {
    return nullptr;
  }

  const proto::Proto3Any& custom_data = output_data.custom();

  auto scanner_output = std::make_unique<proto::ScannerOutput>();
  scanner_output->ParseFromString(custom_data.value());

  return scanner_output;
}

void OnServerResponseOrErrorReceived(
    ScannerProvider::ScannerProtoResponseCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  // Check for Manta error status.
  if (manta_status.status_code != MantaStatusCode::kOk) {
    std::move(callback).Run(nullptr, std::move(manta_status));
    return;
  }

  // Check for a valid Manta response.
  if (manta_response == nullptr) {
    std::move(callback).Run(nullptr, {MantaStatusCode::kMalformedResponse});
    return;
  }

  // Check that the Manta response has output data.
  if (manta_response->output_data_size() == 0) {
    std::string message;

    // If there are no outputs, the response might have been filtered.
    if (manta_response->filtered_data_size() > 0 &&
        manta_response->filtered_data(0).is_output_data()) {
      message = base::StrCat({"filtered output for: ",
                              proto::FilteredReason_Name(
                                  manta_response->filtered_data(0).reason())});
    }
    std::move(callback).Run(nullptr,
                            {MantaStatusCode::kBlockedOutputs, message});
    return;
  }

  // Try to extract a ScannerOutput object from the Manta response.
  std::unique_ptr<proto::ScannerOutput> scanner_output =
      GetScannerOutput(*manta_response);
  if (scanner_output == nullptr) {
    std::move(callback).Run(nullptr, {MantaStatusCode::kMalformedResponse});
    return;
  }

  // All checks passed, run callback with valid ScannerOutput.
  std::move(callback).Run(std::move(scanner_output), std::move(manta_status));
}

}  // namespace

ScannerProvider::ScannerProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const ProviderParams& provider_params)
    : BaseProvider(url_loader_factory, identity_manager, provider_params) {}

ScannerProvider::~ScannerProvider() = default;

void ScannerProvider::Call(
    const manta::proto::ScannerInput& scanner_input,
    ScannerProvider::ScannerProtoResponseCallback done_callback) {
  // Check for valid ScannerInput.
  if (!IsValidScannerInput(scanner_input)) {
    std::move(done_callback).Run(nullptr, {MantaStatusCode::kInvalidInput});
    return;
  }
  // Populate Manta request with the specified ScannerInput as the custom input
  // data.
  proto::Request request;
  request.set_feature_name(proto::FeatureName::CHROMEOS_SCANNER);

  proto::InputData* input_data = request.add_input_data();
  input_data->set_tag("scanner_input");

  proto::Proto3Any& custom = *input_data->mutable_custom();
  custom.set_type_url(kScannerInputTypeUrl);
  custom.set_value(scanner_input.SerializeAsString());

  RequestInternal(
      GURL{GetProviderEndpoint(features::IsScannerUseProdServerEnabled())},
      /*annotation_tag=*/kTrafficAnnotation, request, MantaMetricType::kScanner,
      base::BindOnce(&OnServerResponseOrErrorReceived,
                     std::move(done_callback)),
      kTimeout);
}

}  // namespace manta
