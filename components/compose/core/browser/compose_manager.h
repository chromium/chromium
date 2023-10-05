// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_

#include "components/autofill/core/browser/autofill_compose_delegate.h"

namespace compose {

// The interface for embedder-independent, tab-specific compose logic.
class ComposeManager : public autofill::AutofillComposeDelegate {
 public:
  // TODO(b/300325327): Add non-Autofill specific methods.
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_
