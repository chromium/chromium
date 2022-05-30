// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SEMANTIC_ELEMENT_FINDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SEMANTIC_ELEMENT_FINDER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/base_element_finder.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/autofill_assistant/content/common/autofill_assistant_types.mojom.h"
#include "components/autofill_assistant/content/common/node_data.h"

namespace content {
class WebContents;
class RenderFrameHost;
struct GlobalRenderFrameHostId;
}  // namespace content

namespace autofill_assistant {
class DevtoolsClient;
class ElementFinderResult;

class SemanticElementFinder : public BaseElementFinder {
 public:
  SemanticElementFinder(content::WebContents* web_contents,
                        DevtoolsClient* devtools_client,
                        AnnotateDomModelService* annotate_dom_model_service,
                        const Selector& selector);
  ~SemanticElementFinder() override;

  SemanticElementFinder(const SemanticElementFinder&) = delete;
  SemanticElementFinder& operator=(const SemanticElementFinder&) = delete;

  void Start(const ElementFinderResult& start_element,
             BaseElementFinder::Callback callback) override;

  ElementFinderInfoProto GetLogInfo() const override;

  // Returns the backend node id of the first result (if any), or 0.
  int GetBackendNodeId() const override;

 private:
  // Returns the given status and no element. This expects an error status.
  void GiveUpWithError(const ClientStatus& status);

  // Builds a result from the |render_frame_host|, the |object_id| and the
  // |backend_node_id| returns it withan ok status.
  void ResultFound(content::RenderFrameHost* render_frame_host,
                   const std::string& object_id,
                   int backend_node_id);

  // Call |callback_| with the |status| and |result|.
  void SendResult(const ClientStatus& status,
                  const ElementFinderResult& result);

  // Run the model annotation on all frames for the current |start_frame|.
  void RunAnnotateDomModel(content::RenderFrameHost* start_frame);

  // Runs the model on the frame identified by |host_id|.
  void RunAnnotateDomModelOnFrame(
      const content::GlobalRenderFrameHostId& host_id,
      base::OnceCallback<void(std::vector<GlobalBackendNodeId>)> callback);
  void OnRunAnnotateDomModelOnFrame(
      const content::GlobalRenderFrameHostId& host_id,
      base::OnceCallback<void(std::vector<GlobalBackendNodeId>)> callback,
      mojom::NodeDataStatus status,
      const std::vector<NodeData>& node_data);

  // Called once the model has been run on all frames.
  void OnRunAnnotateDomModel(
      const std::vector<std::vector<GlobalBackendNodeId>>& all_nodes);

  void OnResolveNodeForAnnotateDom(
      GlobalBackendNodeId node,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<dom::ResolveNodeResult> result);

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<DevtoolsClient> devtools_client_;
  const raw_ptr<AnnotateDomModelService> annotate_dom_model_service_;
  const Selector selector_;
  BaseElementFinder::Callback callback_;

  // Elements gathered through all frames. Unused if the |selector_| does not
  // contain |SemanticInformation|.
  std::vector<GlobalBackendNodeId> semantic_node_results_;
  std::vector<mojom::NodeDataStatus> node_data_frame_status_;

  base::WeakPtrFactory<SemanticElementFinder> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SEMANTIC_ELEMENT_FINDER_H_
