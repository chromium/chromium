// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
#include "components/autofill/core/common/password_form.h"
#include "third_party/blink/public/platform/web_string.h"
#include "url/gurl.h"

namespace blink {
class WebFormElement;
class WebLocalFrame;
}

namespace re2 {
class RE2;
}

namespace autofill {

struct PasswordForm;
class FieldDataManager;

// The caller of this function is responsible for deleting the returned object.
re2::RE2* CreateMatcher(void* instance, const char* pattern);

// Tests whether the given form is a GAIA reauthentication form.
bool IsGaiaReauthenticationForm(const blink::WebFormElement& form);

// Tests whether the given form is a GAIA form with a skip password argument.
bool IsGaiaWithSkipSavePasswordForm(const blink::WebFormElement& form);

// Creates a |PasswordForm| from DOM which only contains the |form_data| as well
// as origin, action and gaia flags.
std::unique_ptr<PasswordForm> CreateSimplifiedPasswordFormFromWebForm(
    const blink::WebFormElement& form,
    const FieldDataManager* field_data_manager,
    UsernameDetectorCache* username_detector_cache);

// Same as CreateSimlePasswordFormFromWebForm() but for input elements that are
// not enclosed in <form> element.
std::unique_ptr<PasswordForm>
CreateSimplifiedPasswordFormFromUnownedInputElements(
    const blink::WebLocalFrame& frame,
    const FieldDataManager* field_data_manager,
    UsernameDetectorCache* username_detector_cache);

// The "Realm" for the sign-on. This is scheme, host, port.
std::string GetSignOnRealm(const GURL& origin);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H__
