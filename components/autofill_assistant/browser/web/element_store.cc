// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/element_store.h"

#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

ElementStore::ElementStore(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ElementStore::~ElementStore() = default;

void ElementStore::AddElement(const std::string& client_id,
                              const DomObjectFrameStack& object) {
  object_map_[client_id] = object;
}

ClientStatus ElementStore::GetElement(const std::string& client_id,
                                      ElementFinderResult* out_element) const {
  DCHECK(out_element != nullptr);
  auto it = object_map_.find(client_id);
  if (it == object_map_.end()) {
    return ClientStatus(CLIENT_ID_RESOLUTION_FAILED);
  }

  return RestoreElement(it->second, out_element);
}

ClientStatus ElementStore::RestoreElement(
    const DomObjectFrameStack& object,
    ElementFinderResult* out_element) const {
  out_element->SetObjectId(object.object_data.object_id);
  out_element->SetNodeFrameId(object.object_data.node_frame_id);
  out_element->SetFrameStack(object.frame_stack);
  auto* frame = FindCorrespondingRenderFrameHost(
      object.object_data.node_frame_id, web_contents_);
  if (frame == nullptr) {
    VLOG(1) << __func__ << " failed to resolve frame.";
    return ClientStatus(CLIENT_ID_RESOLUTION_FAILED);
  }
  out_element->SetRenderFrameHost(frame);
  return OkClientStatus();
}

bool ElementStore::RemoveElement(const std::string& client_id) {
  return object_map_.erase(client_id);
}

bool ElementStore::HasElement(const std::string& client_id) const {
  return object_map_.find(client_id) != object_map_.end();
}

void ElementStore::Clear() {
  object_map_.clear();
}

}  // namespace autofill_assistant
