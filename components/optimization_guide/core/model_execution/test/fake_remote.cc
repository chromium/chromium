// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_remote.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "components/optimization_guide/core/model_execution/execute_remote_fn.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

// A remote fallback that is unsuccessful.
void BadRequestRemote(ModelBasedCapabilityKey key,
                      const google::protobuf::MessageLite& req,
                      std::optional<base::TimeDelta> timeout,
                      std::unique_ptr<proto::LogAiDataRequest> log,
                      OptimizationGuideModelExecutionResultCallback callback) {
  std::move(callback).Run(
      OptimizationGuideModelExecutionResult(
          base::unexpected(
              OptimizationGuideModelExecutionError::FromHttpStatusCode(
                  net::HTTP_BAD_REQUEST)),
          nullptr),
      nullptr);
}

// A remote callback that fails the test if it is used.
void FailRemote(ModelBasedCapabilityKey key,
                const google::protobuf::MessageLite& req,
                std::optional<base::TimeDelta> timeout,
                std::unique_ptr<proto::LogAiDataRequest> log,
                OptimizationGuideModelExecutionResultCallback callback) {
  ADD_FAILURE() << "Unexpected use of remote fallback";
  BadRequestRemote(key, req, timeout, std::move(log), std::move(callback));
}

}  // namespace

ExecuteRemoteFn FailOnRemoteFallback() {
  return base::BindRepeating(&FailRemote);
}

ExpectedRemoteFallback::FallbackArgs::FallbackArgs() = default;
ExpectedRemoteFallback::FallbackArgs::FallbackArgs(FallbackArgs&&) = default;
ExpectedRemoteFallback::FallbackArgs::~FallbackArgs() = default;

ExpectedRemoteFallback::FallbackArgs&
ExpectedRemoteFallback::FallbackArgs::operator=(FallbackArgs&&) = default;

ExpectedRemoteFallback::ExpectedRemoteFallback() = default;
ExpectedRemoteFallback::~ExpectedRemoteFallback() = default;

ExecuteRemoteFn ExpectedRemoteFallback::CreateExecuteRemoteFn() {
  return base::BindLambdaForTesting(
      [&](ModelBasedCapabilityKey feature,
          const google::protobuf::MessageLite& m,
          std::optional<base::TimeDelta> timeout,
          std::unique_ptr<proto::LogAiDataRequest> log,
          OptimizationGuideModelExecutionResultCallback callback) {
        auto request = base::WrapUnique(m.New());
        request->CheckTypeAndMergeFrom(m);
        FallbackArgs args;
        args.feature = feature;
        args.request = std::move(request);
        args.timeout = std::move(timeout);
        args.log = std::move(log);
        args.callback = std::move(callback);
        future_.GetCallback().Run(std::move(args));
      });
}

}  // namespace optimization_guide
