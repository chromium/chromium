// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTIL_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTIL_H_

#include <string>

#include "url/gurl.h"

namespace blink {
class WebFormElement;
}

namespace re2 {
class RE2;
}

namespace autofill {

// The caller of this function is responsible for deleting the returned object.
re2::RE2* CreateMatcher(void* instance, const char* pattern);

// Tests whether the given form is a GAIA reauthentication form.
bool IsGaiaReauthenticationForm(const blink::WebFormElement& form);

// Tests whether the given form is a GAIA form with a skip password argument.
bool IsGaiaWithSkipSavePasswordForm(const blink::WebFormElement& form);

// The "Realm" for the sign-on. This is scheme, host, port.
std::string GetSignOnRealm(const GURL& origin);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTIL_H_
