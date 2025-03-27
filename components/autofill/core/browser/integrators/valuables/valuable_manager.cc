// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/valuables/valuable_manager.h"

namespace autofill {

ValuableManager::ValuableManager() = default;

ValuableManager::~ValuableManager() = default;

void ValuableManager::FetchValue(
    ValuableId valuable_id,
    OnValuableFetchedCallback on_valuable_fetched) {
  // TODO(crbug.com/405371277): Implement.
  NOTIMPLEMENTED();
}

}  // namespace autofill
