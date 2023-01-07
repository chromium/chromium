// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_view.h"

// Factory function to create PasswordChangeRun for desktop. Today
// apc_client_impl.cc will make sure to finish the script and get rid of the
// pointer to the this display/view once its closed.
base::WeakPtr<PasswordChangeRunDisplay> PasswordChangeRunDisplay::Create(
    base::WeakPtr<PasswordChangeRunController> controller,
    raw_ptr<AssistantDisplayDelegate> display_delegate) {
  return (new PasswordChangeRunView(controller, display_delegate))
      ->GetWeakPtr();
}
