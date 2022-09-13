// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_

#include "build/build_config.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

// Structure used for sending information from browser to renderer about on
// which fields password should be generated.
struct PasswordFormGenerationData {
#if BUILDFLAG(IS_IOS)
  FormRendererId form_renderer_id;
#endif
  FieldRendererId new_password_renderer_id;
  FieldRendererId confirmation_password_renderer_id;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_FORM_GENERATION_DATA_H_
