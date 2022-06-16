// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_RESULT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_RESULT_H_

#include <string>
#include <vector>

#include "components/autofill_assistant/browser/web/element.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class RenderFrameHost;
struct GlobalRenderFrameHostId;
}  // namespace content

namespace autofill_assistant {

// ElementFinderResult is the fully resolved element that can be used without
// limitations. This means that |render_frame_host()| has been found and is not
// nullptr.
class ElementFinderResult {
 public:
  ElementFinderResult();
  ~ElementFinderResult();
  ElementFinderResult(const ElementFinderResult&);

  // Create an instance that is deemed to be empty. This can be used for
  // optional Elements (e.g. optional an frame).
  static ElementFinderResult EmptyResult();

  const DomObjectFrameStack& dom_object() const { return dom_object_; }

  content::RenderFrameHost* render_frame_host() const {
    return content::RenderFrameHost::FromID(dom_object_.render_frame_id);
  }

  const std::string& object_id() const {
    return dom_object_.object_data.object_id;
  }

  absl::optional<int> backend_node_id() const {
    return dom_object_.object_data.backend_node_id;
  }

  const std::string& node_frame_id() const {
    return dom_object_.object_data.node_frame_id;
  }

  const std::vector<JsObjectIdentifier>& frame_stack() const {
    return dom_object_.frame_stack;
  }

  bool IsEmpty() const {
    return object_id().empty() && node_frame_id().empty();
  }

#if defined(UNIT_TEST)
  void SetRenderFrameHostForTest(content::RenderFrameHost* render_frame_host) {
    if (!render_frame_host) {
      return;
    }
    SetRenderFrameHostGlobalId(render_frame_host->GetGlobalId());
  }
#endif  // defined(UNIT_TEST)

  void SetRenderFrameHostGlobalId(
      content::GlobalRenderFrameHostId render_frame_id) {
    dom_object_.render_frame_id = render_frame_id;
  }

  void SetObjectId(const std::string& object_id) {
    dom_object_.object_data.object_id = object_id;
  }

  void SetBackendNodeId(absl::optional<int> backend_node_id) {
    dom_object_.object_data.backend_node_id = backend_node_id;
  }

  void SetNodeFrameId(const std::string& node_frame_id) {
    dom_object_.object_data.node_frame_id = node_frame_id;
  }

  void SetFrameStack(const std::vector<JsObjectIdentifier>& frame_stack) {
    dom_object_.frame_stack = frame_stack;
  }

 private:
  DomObjectFrameStack dom_object_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_FINDER_RESULT_H_
