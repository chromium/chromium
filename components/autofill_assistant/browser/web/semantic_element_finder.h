// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SEMANTIC_ELEMENT_FINDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SEMANTIC_ELEMENT_FINDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/base_element_finder.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/content/browser/annotate_dom_model_service.h"
#include "components/autofill_assistant/content/common/autofill_assistant_types.mojom.h"
#include "components/autofill_assistant/content/common/node_data.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
class RenderFrameHost;
struct GlobalRenderFrameHostId;
}  // namespace content

namespace autofill_assistant {
class DevtoolsClient;
class ElementFinderResult;

class SemanticElementFinder : public BaseElementFinder,
                              public content::WebContentsObserver {
 public:
  struct SemanticNodeResult {
    GlobalBackendNodeId id = GlobalBackendNodeId(nullptr, -1);
    bool used_override = false;
  };

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

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  // Returns the given status and no element. This expects an error status.
  void GiveUpWithError(const ClientStatus& status);

  // Builds a result from the provided information and returns it with an
  // ok status.
  void ResultFound(const SemanticNodeResult& node_id,
                   const std::string& object_id,
                   const std::string& devtools_frame_id);

  // Call |callback_| with the |status| and |result|.
  // The callback may cause a deletion of this object.
  void SendResult(const ClientStatus& status,
                  const ElementFinderResult& result);

  // Run the model annotation on all frames for the current |start_frame|.
  void RunAnnotateDomModel(content::RenderFrameHost* start_frame);

  // Runs the model on the frame identified by |host_id|.
  void RunAnnotateDomModelOnFrame(
      const content::GlobalRenderFrameHostId& host_id);
  void OnRunAnnotateDomModelOnFrame(
      const content::GlobalRenderFrameHostId& host_id,
      mojom::NodeDataStatus status,
      const std::vector<NodeData>& node_data);

  // Called once the model has been run on all frames, or when the timeout has
  // occurred.
  void OnRunAnnotateDomModel();

  void OnResolveNodeForAnnotateDom(
      const SemanticNodeResult& node,
      const std::string& devtools_frame_id,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<dom::ResolveNodeResult> result);

  // |MarkRenderFrameProcessed| and |MarkAllRenderFramesProcessed| mark frames
  // as processed. If no unprocessed frames are left, the pending timeout is
  // cancelled and |Finalize| is called.
  void MarkRenderFrameProcessed(content::GlobalRenderFrameHostId host_id);

  // Cancel the pending timeout and call |OnRunAnnotateDomModel|.
  void Finalize();

  // Returns true if we expect a call for the given |host_id|.
  bool IsRenderFrameExpected(content::GlobalRenderFrameHostId host_id);

  void OnTimeout();

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<DevtoolsClient> devtools_client_;
  const raw_ptr<AnnotateDomModelService> annotate_dom_model_service_;
  const Selector selector_;
  SelectorProto::SemanticFilter filter_;
  BaseElementFinder::Callback callback_;

  // Elements gathered through all frames. Unused if the |selector_| does not
  // contain |SemanticInformation|.
  std::vector<SemanticNodeResult> semantic_node_results_;
  std::vector<mojom::NodeDataStatus> node_data_frame_status_;

  std::set<content::GlobalRenderFrameHostId> expected_frame_ids_;
  std::map<content::GlobalRenderFrameHostId, std::vector<SemanticNodeResult>>
      received_results_;

  std::unique_ptr<base::OneShotTimer> timer_ = nullptr;

  base::WeakPtrFactory<SemanticElementFinder> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SEMANTIC_ELEMENT_FINDER_H_
