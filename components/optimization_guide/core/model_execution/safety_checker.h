// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CHECKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CHECKER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/safety_config.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

// Getter for shared text safety model.
class TextSafetyClient {
 public:
  virtual ~TextSafetyClient() = 0;
  virtual mojo::Remote<on_device_model::mojom::TextSafetyModel>&
  GetTextSafetyModelRemote() = 0;
};

// Performs safety checks according to a config against a text safety model.
class SafetyChecker final {
 public:
  struct Result {
    Result();
    Result(const Result&);
    Result(Result&&);
    Result& operator=(Result&&);
    ~Result();

    static Result Merge(std::vector<Result> results);

    bool failed_to_run = false;
    bool is_unsafe = false;
    bool is_unsupported_language = false;
    google::protobuf::RepeatedPtrField<
        proto::InternalOnDeviceModelExecutionInfo>
        logs;
  };
  using ResultCallback = base::OnceCallback<void(Result)>;

  // TODO(holte): Have this own a TextSafetyClient ref.
  explicit SafetyChecker(SafetyConfig safety_cfg);
  ~SafetyChecker();

  // Runs all of the configured request checks for a request.
  void RunRequestChecks(TextSafetyClient& client,
                        const google::protobuf::MessageLite& request_metadata,
                        ResultCallback callback);

  // Runs the configured check (if any) for evaluating raw output.
  void RunRawOutputCheck(TextSafetyClient& client,
                         const std::string& raw_output,
                         ResultCallback callback);

  // Runs all of the configured checks for evaluating parsed responses.
  void RunResponseChecks(TextSafetyClient& client,
                         const google::protobuf::MessageLite& request,
                         const proto::Any& response,
                         ResultCallback callback);

  const SafetyConfig& safety_cfg() const { return safety_cfg_; }

  size_t TokenInterval() const { return safety_cfg_.TokenInterval(); }

 private:
  SafetyConfig safety_cfg_;
  base::WeakPtrFactory<SafetyChecker> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CHECKER_H_
