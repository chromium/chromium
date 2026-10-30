// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_runner.h"

#include <optional>
#include <string>

#include "base/check_deref.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/integrators/custom_form_controls/custom_form_control_script_index.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

namespace autofill {

CustomFormControlScriptRunner::CustomFormControlScriptRunner(
    AutofillClient& client,
    const CustomFormControlScriptIndex* index)
    : index_(CHECK_DEREF(index)) {
  autofill_managers_observation_.Observe(&client);
}

CustomFormControlScriptRunner::~CustomFormControlScriptRunner() = default;

void CustomFormControlScriptRunner::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillManager::LifecycleState previous,
    AutofillManager::LifecycleState new_state) {
  using enum AutofillManager::LifecycleState;
  // The frame's document is about to change, or the frame is going away, so
  // the next document needs its own lookup.
  if (new_state == kPendingReset || new_state == kPendingDeletion) {
    frames_looked_up_.erase(manager.driver().GetFrameToken());
  }
}

void CustomFormControlScriptRunner::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  // Look up each document once: the first time forms are seen in it while its
  // frame is active (e.g. not BFCached) and the index has read its directory.
  // Until the index has read its directory, it has no entries, so a document
  // whose forms are all seen before then isn't looked up.
  if (updated_forms.empty() || !manager.driver().IsActive() ||
      !index_->is_initialized() ||
      !frames_looked_up_.insert(manager.driver().GetFrameToken()).second) {
    return;
  }
  const GURL& url = static_cast<ContentAutofillDriver&>(manager.driver())
                        .render_frame_host()
                        ->GetLastCommittedURL();
  std::optional<std::string> script = index_->FindScript(url);
  if (!script) {
    return;
  }
  if (script_found_callback_for_testing_) {
    script_found_callback_for_testing_.Run(*script);
  }
  // TODO(b/578383598): Load `script` into the frame's renderer and detect the
  // document's custom form controls.
}

}  // namespace autofill
