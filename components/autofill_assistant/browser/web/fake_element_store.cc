// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/fake_element_store.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

FakeElementStore::FakeElementStore()
    : ElementStore(nullptr), web_contents_(nullptr) {}

FakeElementStore::FakeElementStore(content::WebContents* web_content)
    : ElementStore(web_content), web_contents_(web_content) {}

FakeElementStore::~FakeElementStore() = default;

ClientStatus FakeElementStore::GetElement(
    const std::string& client_id,
    ElementFinderResult* out_element) const {
  auto it = object_map_.find(client_id);
  if (it == object_map_.end()) {
    return ClientStatus(CLIENT_ID_RESOLUTION_FAILED);
  }

  out_element->SetObjectId(it->second.object_data.object_id);
  out_element->SetNodeFrameId(it->second.object_data.node_frame_id);
  out_element->SetFrameStack(it->second.frame_stack);
  if (web_contents_ != nullptr) {
    out_element->SetRenderFrameHostForTest(
        web_contents_->GetPrimaryMainFrame());
  }
  return OkClientStatus();
}

}  // namespace autofill_assistant
