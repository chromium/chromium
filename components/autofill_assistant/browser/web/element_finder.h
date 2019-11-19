// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_dom.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/devtools/devtools_client.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/web_controller_worker.h"

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace autofill_assistant {
class DevtoolsClient;

// Worker class to find element(s) matching a selector.
class ElementFinder : public WebControllerWorker {
 public:
  struct Result {
    Result() = default;
    ~Result() = default;

    // The render frame host contains the element.
    content::RenderFrameHost* container_frame_host;

    // The selector index in the given selectors corresponding to the container
    // frame. Zero indicates the element is in main frame or the first element
    // is the container frame selector. Compare main frame with the above
    // |container_frame_host| to distinguish them.
    size_t container_frame_selector_index;

    // The object id of the element.
    std::string object_id;

    // The id of the frame the element's node is in.
    std::string node_frame_id;
  };

  // |web_contents| and |devtools_client| must be valid for the lifetime of the
  // instance.
  ElementFinder(content::WebContents* web_contents_,
                DevtoolsClient* devtools_client,
                const Selector& selector,
                bool strict);
  ~ElementFinder() override;

  using Callback =
      base::OnceCallback<void(const ClientStatus&, std::unique_ptr<Result>)>;

  // Finds the element and calls the callback.
  void Start(Callback callback_);

 private:
  void SendResult(const ClientStatus& status);
  void OnGetDocumentElement(size_t index,
                            const DevtoolsClient::ReplyStatus& reply_status,
                            std::unique_ptr<runtime::EvaluateResult> result);
  void RecursiveFindElement(const std::string& object_id, size_t index);
  void OnQuerySelectorAll(
      size_t index,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnDescribeNodeForPseudoElement(
      dom::PseudoType pseudo_type,
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNodeForPseudoElement(
      const DevtoolsClient::ReplyStatus& reply_status,
      std::unique_ptr<dom::ResolveNodeResult> result);
  void OnDescribeNode(const std::string& object_id,
                      size_t index,
                      const DevtoolsClient::ReplyStatus& reply_status,
                      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNode(size_t index,
                     const DevtoolsClient::ReplyStatus& reply_status,
                     std::unique_ptr<dom::ResolveNodeResult> result);

  content::RenderFrameHost* FindCorrespondingRenderFrameHost(
      std::string frame_id);

  content::WebContents* const web_contents_;
  DevtoolsClient* const devtools_client_;
  const Selector selector_;

  const bool strict_;
  Callback callback_;
  std::unique_ptr<Result> element_result_;

  base::WeakPtrFactory<ElementFinder> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_H_
