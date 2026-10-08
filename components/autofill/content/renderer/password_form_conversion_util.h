// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTIL_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTIL_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
#include "components/autofill/content/renderer/synchronous_form_cache.h"
#include "components/autofill/content/renderer/timing.h"
#include "third_party/blink/public/platform/web_string.h"
#include "url/gurl.h"

namespace blink {
class WebDocument;
class WebFormElement;
}

namespace re2 {
class RE2;
}

namespace autofill {

class FieldDataManager;

// The caller of this function is responsible for deleting the returned object.
re2::RE2* CreateMatcher(void* instance, const char* pattern);

// Tests whether the given form is a GAIA reauthentication form.
bool IsGaiaReauthenticationForm(const blink::WebFormElement& form);

// Tests whether the given form is a GAIA form with a skip password argument.
bool IsGaiaWithSkipSavePasswordForm(const blink::WebFormElement& form);

// Extracts `FormData` for the form with `form_id` (or the unowned form if
// `form_id.is_null()`) in `document`, and populates the additional states that
// are useful for the password manager.
std::optional<FormData> CreateFormData(
    const blink::WebDocument& document,
    FormRendererId form_id,
    const FieldDataManager& field_data_manager,
    UsernameDetectorCache* username_detector_cache,
    form_util::ButtonTitlesCache* button_titles_cache,
    const CallTimerState& timer_state,
    const SynchronousFormCache& form_cache);

// The "Realm" for the sign-on. This is scheme, host, port.
std::string GetSignOnRealm(const GURL& origin);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTIL_H_
