// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_simple_task_environment.h"

#include "components/metrics/private_metrics/private_insights/fcp_http_client.h"
#include "third_party/federated_compute/chromium/fcp/client/attestation/attestation_transparency_verifier.h"
#include "third_party/federated_compute/src/fcp/client/attestation/attestation_verifier.h"
#include "third_party/federated_compute/src/fcp/protos/confidentialcompute/access_policy_endorsement_options.pb.h"

namespace private_insights {

namespace {

// Iterator that returns a single string then finishes.
class SingleExampleIterator : public fcp::client::ExampleIterator {
 public:
  explicit SingleExampleIterator(std::string data) : data_(std::move(data)) {}

  absl::StatusOr<std::string> Next() override {  // nocheck
    if (returned_) {
      return absl::OutOfRangeError("End of iterator");
    }
    returned_ = true;
    return data_;
  }

  void Close() override {}

 private:
  std::string data_;
  bool returned_ = false;
};

}  // namespace

FcpSimpleTaskEnvironment::FcpSimpleTaskEnvironment(
    std::string base_dir,
    std::string cache_dir,
    std::unique_ptr<FcpHttpRequestManager> http_request_manager,
    bool use_attestation_transparency_verifier)
    : base_dir_(std::move(base_dir)),
      cache_dir_(std::move(cache_dir)),
      http_request_manager_(std::move(http_request_manager)),
      use_attestation_transparency_verifier_(
          use_attestation_transparency_verifier) {}

FcpSimpleTaskEnvironment::~FcpSimpleTaskEnvironment() = default;

std::string FcpSimpleTaskEnvironment::GetBaseDir() {
  return base_dir_;
}

std::string FcpSimpleTaskEnvironment::GetCacheDir() {
  return cache_dir_;
}

absl::StatusOr<std::unique_ptr<fcp::client::ExampleIterator>>  // nocheck
FcpSimpleTaskEnvironment::CreateExampleIterator(
    const google::internal::federated::plan::ExampleSelector&
        example_selector) {
  return std::make_unique<SingleExampleIterator>(result_.SerializeAsString());
}

bool FcpSimpleTaskEnvironment::TrainingConditionsSatisfied() {
  return true;
}

std::unique_ptr<fcp::client::http::HttpClient>
FcpSimpleTaskEnvironment::CreateHttpClient() {
  return std::make_unique<FcpHttpClient>(http_request_manager_.get());
}

std::unique_ptr<fcp::client::attestation::AttestationVerifier>
FcpSimpleTaskEnvironment::CreateAttestationVerifier() {
  if (use_attestation_transparency_verifier_) {
    return std::make_unique<
        fcp::client::attestation::AttestationTransparencyVerifier>(
        fcp::confidentialcompute::AccessPolicyEndorsementOptions::
            default_instance());
  } else {
    return std::make_unique<
        fcp::client::attestation::AlwaysPassingAttestationVerifier>();
  }
}

}  // namespace private_insights
