// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autofill_test_utils.h"

#include <utility>
#include <vector>

#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_frame_host.h"

namespace autofill::test {

FormData CreateFormDataForRenderFrameHost(content::RenderFrameHost& rfh,
                                          std::vector<FormFieldData> fields) {
  FormData form;
  form.set_url(rfh.GetLastCommittedURL());
  form.set_action(form.url());
  form.set_host_frame(LocalFrameToken(rfh.GetFrameToken().value()));
  form.set_renderer_id(MakeFormRendererId());
  for (FormFieldData& field : fields) {
    field.set_host_frame(form.host_frame());
  }
  form.set_fields(std::move(fields));
  return form;
}

}  // namespace autofill::test
