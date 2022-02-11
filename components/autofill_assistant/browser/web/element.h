// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_H_

#include <string>
#include <vector>

#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// JsObjectIdentifier contains all data required to address an object in a
// JavaScript context via Devtools.
struct JsObjectIdentifier {
  // The object id in the JavaScript context. This can be a JavaScript node
  // instance or an array of such.
  std::string object_id;

  // The frame id to use to execute devtools Javascript calls within the
  // context of the frame. Might be empty if no frame id needs to be
  // specified.
  std::string node_frame_id;
};

// DomObjectFrameStack contains all data required to use an object including
// its nesting in frames information.
struct DomObjectFrameStack {
  DomObjectFrameStack();
  ~DomObjectFrameStack();
  DomObjectFrameStack(const DomObjectFrameStack&);

  // The data for the final object.
  JsObjectIdentifier object_data;

  // This holds the information of all the frames that were traversed until
  // the final element was reached.
  std::vector<JsObjectIdentifier> frame_stack;
};

// GlobalBackendNodeId contains all data required to uniquely identify a node.
class GlobalBackendNodeId {
 public:
  GlobalBackendNodeId(content::RenderFrameHost* render_frame_host,
                      int backend_node_id);
  GlobalBackendNodeId(content::GlobalRenderFrameHostId host_id,
                      int backend_node_id);
  ~GlobalBackendNodeId();
  GlobalBackendNodeId(const GlobalBackendNodeId&);

  bool operator==(const GlobalBackendNodeId& other) const;

  content::GlobalRenderFrameHostId host_id() const;
  int backend_node_id() const;

 private:
  content::GlobalRenderFrameHostId host_id_;
  int backend_node_id_ = -1;
};

// Find the frame host in the set of known frames matching the |frame_id|. This
// returns nullptr if no frame is found.
content::RenderFrameHost* FindCorrespondingRenderFrameHost(
    const std::string& frame_id,
    content::WebContents* web_contents);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_ELEMENT_H_
