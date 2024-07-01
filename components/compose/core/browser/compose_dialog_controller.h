// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_DIALOG_CONTROLLER_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_DIALOG_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "components/compose/core/browser/compose_client.h"

namespace compose {

// The interface for embedder-independent dialog controllers.
class ComposeDialogController {
 public:
  virtual ~ComposeDialogController() = default;

  // Shows the compose dialog. `focus_lost_callback` is called when the compose
  // dialog loses focus.
  virtual void ShowUI(base::OnceClosure focus_lost_callback) = 0;

  // Closes the compose dialog.
  virtual void Close() = 0;

  // Returns true when the dialog is showing and false otherwise.
  virtual bool IsDialogShowing() = 0;

  // Returns an identifier for the field that this dialog is acting upon.
  virtual const ComposeClient::FieldIdentifier& GetFieldIds() = 0;
};

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_COMPOSE_DIALOG_CONTROLLER_H_
