// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_provider.h"

#include "components/autofill/core/browser/android_autofill_manager.h"

namespace autofill {
namespace {
bool g_is_download_manager_disabled_for_testing = false;
}

// static
bool AutofillProvider::is_download_manager_disabled_for_testing() {
  return g_is_download_manager_disabled_for_testing;
}

void AutofillProvider::set_is_download_manager_disabled_for_testing() {
  g_is_download_manager_disabled_for_testing = true;
}

AutofillProvider::AutofillProvider() {}

AutofillProvider::~AutofillProvider() {}

void AutofillProvider::SendFormDataToRenderer(AndroidAutofillManager* manager,
                                              int requestId,
                                              const FormData& formData) {
  manager->SendFormDataToRenderer(
      requestId, AutofillDriver::FORM_DATA_ACTION_FILL, formData);
}

void AutofillProvider::RendererShouldAcceptDataListSuggestion(
    AndroidAutofillManager* manager,
    const FieldGlobalId& field_id,
    const std::u16string& value) {
  manager->driver()->RendererShouldAcceptDataListSuggestion(field_id, value);
}

}  // namespace autofill
