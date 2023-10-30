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
  // Opens compose from the context menu given the 'driver',
  // 'form_renderer_id', and 'field_renderer_id'.
  virtual void OpenComposeFromContextMenu(
      autofill::AutofillDriver* driver,
      const autofill::FormRendererId form_renderer_id,
      const autofill::FieldRendererId field_renderer_id) = 0;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_MANAGER_H_
