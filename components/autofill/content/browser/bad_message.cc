// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/bad_message.h"

#include "base/containers/contains.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/browser/render_frame_host.h"

namespace autofill::bad_message {

namespace internal {

bool CheckSingleValidTriggerSource(
    AutofillSuggestionTriggerSource trigger_source) {
  if (trigger_source ==
      AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess) {
    mojo::ReportBadMessage(
        "PlusAddressUpdatedInBrowserProcess is not a permitted trigger source "
        "in the renderer");
    return false;
  }
  return true;
}

bool CheckFieldInForm(const FormData& form, FieldRendererId field_id) {
  if (!base::Contains(form.fields(), field_id, &FormFieldData::renderer_id)) {
    mojo::ReportBadMessage("Unexpected FormData/FieldRendererId pair received");
    return false;
  }
  return true;
}

}  // namespace internal

bool CheckFrameNotPrerendering(content::RenderFrameHost* frame) {
  if (frame->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    mojo::ReportBadMessage("Autofill is not allowed in a prerendering frame");
    return false;
  }
  return true;
}

}  // namespace autofill::bad_message
