// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/fake_variations_client.h"

namespace contextual_search {

bool FakeVariationsClient::IsOffTheRecord() const {
  return false;
}

variations::mojom::VariationsHeadersPtr
FakeVariationsClient::GetVariationsHeaders() const {
  return variations::mojom::VariationsHeaders::New();
}

}  // namespace contextual_search
