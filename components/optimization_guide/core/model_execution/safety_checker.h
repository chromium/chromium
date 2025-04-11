// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CHECKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CHECKER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/safety_config.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/cpp/text_safety_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

// Getter for shared text safety model.
class TextSafetyClient {
 public:
  virtual ~TextSafetyClient() = 0;
  virtual void StartSession(
      mojo::PendingReceiver<on_device_model::mojom::TextSafetySession>
          session) = 0;
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

  explicit SafetyChecker(base::WeakPtr<TextSafetyClient> client,
                         SafetyConfig safety_cfg);
  SafetyChecker(const SafetyChecker&);
  ~SafetyChecker();

  // Runs all of the configured request checks for a request.
  void RunRequestChecks(const MultimodalMessage& request_metadata,
                        ResultCallback callback);

  // Runs the configured check (if any) for evaluating raw output.
  void RunRawOutputCheck(const std::string& raw_output,
                         ResponseCompleteness completeness,
                         ResultCallback callback);

  // Runs all of the configured checks for evaluating parsed responses.
  void RunResponseChecks(const MultimodalMessage& request,
                         const proto::Any& response,
                         ResponseCompleteness completeness,
                         ResultCallback callback);

  const SafetyConfig& safety_cfg() const { return safety_cfg_; }
  const base::WeakPtr<TextSafetyClient>& client() const { return client_; }

 private:
  mojo::Remote<on_device_model::mojom::TextSafetySession>& GetSession();

  mojo::Remote<on_device_model::mojom::TextSafetySession> session_;

  base::WeakPtr<TextSafetyClient> client_;
  SafetyConfig safety_cfg_;
  base::WeakPtrFactory<SafetyChecker> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CHECKER_H_
