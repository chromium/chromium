// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/autofill_provider.h"

#include "base/memory/ptr_util.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "content/public/browser/web_contents.h"

namespace autofill {
namespace {

bool g_is_download_manager_disabled_for_testing = false;
}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutofillProvider);

bool AutofillProvider::is_download_manager_disabled_for_testing() {
  return g_is_download_manager_disabled_for_testing;
}

void AutofillProvider::set_is_download_manager_disabled_for_testing() {
  g_is_download_manager_disabled_for_testing = true;
}

AutofillProvider::AutofillProvider(content::WebContents* web_contents)
    : content::WebContentsUserData<AutofillProvider>(*web_contents) {
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(this));
}

AutofillProvider::~AutofillProvider() = default;

void AutofillProvider::FillOrPreviewForm(AndroidAutofillManager* manager,
                                         const FormData& formData,
                                         const url::Origin& triggered_origin) {
  manager->FillOrPreviewForm(mojom::RendererFormDataAction::kFill, formData,
                             triggered_origin);
}

void AutofillProvider::RendererShouldAcceptDataListSuggestion(
    AndroidAutofillManager* manager,
    const FieldGlobalId& field_id,
    const std::u16string& value) {
  manager->driver()->RendererShouldAcceptDataListSuggestion(field_id, value);
}

}  // namespace autofill
