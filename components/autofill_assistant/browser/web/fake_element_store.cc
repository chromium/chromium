// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/fake_element_store.h"
#include "components/autofill_assistant/browser/client_status.h"

namespace autofill_assistant {

FakeElementStore::FakeElementStore() : ElementStore(nullptr) {}

FakeElementStore::~FakeElementStore() = default;

ClientStatus FakeElementStore::GetElement(
    const std::string& client_id,
    ElementFinder::Result* out_element) const {
  auto it = object_map_.find(client_id);
  if (it == object_map_.end()) {
    return ClientStatus(CLIENT_ID_RESOLUTION_FAILED);
  }

  out_element->dom_object = it->second;
  return OkClientStatus();
}

}  // namespace autofill_assistant
