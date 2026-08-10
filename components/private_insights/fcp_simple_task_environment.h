// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INSIGHTS_FCP_SIMPLE_TASK_ENVIRONMENT_H_
#define COMPONENTS_PRIVATE_INSIGHTS_FCP_SIMPLE_TASK_ENVIRONMENT_H_

#include "base/memory/ref_counted.h"
#include "third_party/federated_compute/src/fcp/client/attestation/attestation_verifier.h"
#include "third_party/federated_compute/src/fcp/client/example_query_result.pb.h"
#include "third_party/federated_compute/src/fcp/client/simple_task_environment.h"

namespace private_insights {

class FcpHttpRequestManager;

// WARNING: The caller must ensure that this environment outlives the returned
// HttpClient and all network requests initiated through it.
class FcpSimpleTaskEnvironment
    : public fcp::client::SimpleTaskEnvironment,
      public base::RefCountedThreadSafe<FcpSimpleTaskEnvironment> {
 public:
  FcpSimpleTaskEnvironment(
      std::string base_dir,
      std::string cache_dir,
      std::unique_ptr<FcpHttpRequestManager> http_request_manager,
      bool use_attestation_transparency_verifier);

  FcpSimpleTaskEnvironment(const FcpSimpleTaskEnvironment&) = delete;
  FcpSimpleTaskEnvironment& operator=(const FcpSimpleTaskEnvironment&) = delete;

  fcp::client::ExampleQueryResult& result() { return result_; }
  const fcp::client::ExampleQueryResult& result() const { return result_; }

  std::string GetBaseDir() override;
  std::string GetCacheDir() override;

  absl::StatusOr<std::unique_ptr<fcp::client::ExampleIterator>>  // nocheck
  CreateExampleIterator(
      const google::internal::federated::plan::ExampleSelector&
          example_selector) override;

  bool TrainingConditionsSatisfied() override;

  std::unique_ptr<fcp::client::http::HttpClient> CreateHttpClient() override;

  std::unique_ptr<fcp::client::attestation::AttestationVerifier>
  CreateAttestationVerifier() override;

 private:
  friend class base::RefCountedThreadSafe<FcpSimpleTaskEnvironment>;
  ~FcpSimpleTaskEnvironment() override;

  std::string base_dir_;
  std::string cache_dir_;
  std::unique_ptr<FcpHttpRequestManager> http_request_manager_;
  bool use_attestation_transparency_verifier_;

  fcp::client::ExampleQueryResult result_;
};

}  // namespace private_insights

#endif  // COMPONENTS_PRIVATE_INSIGHTS_FCP_SIMPLE_TASK_ENVIRONMENT_H_
