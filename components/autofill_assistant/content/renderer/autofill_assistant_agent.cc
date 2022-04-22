// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_agent.h"

#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/autofill_assistant/node_signals.h"
#include "third_party/blink/public/web/web_local_frame.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/autofill_assistant/content/renderer/autofill_assistant_model_executor.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

namespace autofill_assistant {

AutofillAssistantAgent::AutofillAssistantAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  registry->AddInterface(base::BindRepeating(
      &AutofillAssistantAgent::BindPendingReceiver, base::Unretained(this)));
}

// The destructor is not guaranteed to be called. Destruction happens (only)
// through the OnDestruct() event, which posts a task to delete this object.
// The process may be killed before this deletion can happen.
AutofillAssistantAgent::~AutofillAssistantAgent() = default;

void AutofillAssistantAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillAssistantAgent>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void AutofillAssistantAgent::OnDestruct() {
  delete this;
}

base::WeakPtr<AutofillAssistantAgent> AutofillAssistantAgent::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillAssistantAgent::GetSemanticNodes(
    int32_t role,
    int32_t objective,
    bool ignore_objective,
    base::TimeDelta model_timeout,
    GetSemanticNodesCallback callback) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame) {
    std::move(callback).Run(mojom::NodeDataStatus::kUnexpectedError,
                            std::vector<NodeData>());
    return;
  }

  GetAnnotateDomModel(
      model_timeout,
      base::BindOnce(&AutofillAssistantAgent::OnGetModelFile,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(), frame,
                     role, objective, ignore_objective, std::move(callback)));
}

void AutofillAssistantAgent::GetAnnotateDomModel(
    base::TimeDelta model_timeout,
    base::OnceCallback<void(mojom::ModelStatus, base::File)> callback) {
  GetDriver().GetAnnotateDomModel(model_timeout, std::move(callback));
}

mojom::AutofillAssistantDriver& AutofillAssistantAgent::GetDriver() {
  if (!driver_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&driver_);
  }
  return *driver_;
}

void AutofillAssistantAgent::OnGetModelFile(base::Time start_time,
                                            blink::WebLocalFrame* frame,
                                            int32_t role,
                                            int32_t objective,
                                            bool ignore_objective,
                                            GetSemanticNodesCallback callback,
                                            mojom::ModelStatus model_status,
                                            base::File model) {
  std::vector<NodeData> nodes;
  switch (model_status) {
    case mojom::ModelStatus::kSuccess:
      break;
    case mojom::ModelStatus::kUnexpectedError:
      std::move(callback).Run(mojom::NodeDataStatus::kModelLoadError, nodes);
      return;
    case mojom::ModelStatus::kTimeout:
      std::move(callback).Run(mojom::NodeDataStatus::kModelLoadTimeout, nodes);
      return;
  }

  base::Time on_get_model_file = base::Time::Now();
  DVLOG(3) << "AutofillAssistant, loading model file: "
           << (on_get_model_file - start_time).InMilliseconds() << "ms";

  blink::WebVector<blink::AutofillAssistantNodeSignals> node_signals =
      blink::GetAutofillAssistantNodeSignals(frame->GetDocument());

  base::Time on_node_signals = base::Time::Now();
  DVLOG(3) << "AutofillAssistant, signals extraction: "
           << (on_node_signals - on_get_model_file).InMilliseconds() << "ms";

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  AutofillAssistantModelExecutor model_executor;
  if (!model_executor.InitializeModelFromFile(std::move(model))) {
    std::move(callback).Run(mojom::NodeDataStatus::kInitializationError, nodes);
    return;
  }

  base::Time on_executor_initialized = base::Time::Now();
  DVLOG(3) << "AutofillAssistant, executor initialization: "
           << (on_executor_initialized - on_node_signals).InMilliseconds()
           << "ms";
  DVLOG(3) << "Expected role: " << role << " and objective: " << objective;

  for (const auto& node_signal : node_signals) {
    auto result = model_executor.ExecuteModelWithInput(node_signal);
    DVLOG(3) << "Annotated node with result: role: " << result->first
             << " and objective: " << result->second
             << " (or ignore: " << ignore_objective << ")";
    if (result && result->first == role &&
        (result->second == objective || ignore_objective)) {
      NodeData node_data;
      node_data.backend_node_id = node_signal.backend_node_id;
      nodes.push_back(node_data);
    }
  }

  base::Time on_node_signals_evaluated = base::Time::Now();
  DVLOG(3)
      << "AutofillAssistant, node evaluation (for " << node_signals.size()
      << " nodes): "
      << (on_node_signals_evaluated - on_executor_initialized).InMilliseconds()
      << "ms";
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  std::move(callback).Run(mojom::NodeDataStatus::kSuccess, nodes);
}

}  // namespace autofill_assistant
