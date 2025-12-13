// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_FAKE_VARIATIONS_CLIENT_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_FAKE_VARIATIONS_CLIENT_H_

#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"

namespace contextual_search {

class FakeVariationsClient : public variations::VariationsClient {
 public:
  FakeVariationsClient() = default;
  ~FakeVariationsClient() override = default;

  // variations::VariationsClient:
  bool IsOffTheRecord() const override;
  variations::mojom::VariationsHeadersPtr GetVariationsHeaders() const override;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_FAKE_VARIATIONS_CLIENT_H_
