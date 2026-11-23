// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_util.h"

#include "base/check.h"
#include "base/lazy_instance.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;

namespace autofill {

namespace {

const char kPasswordSiteUrlRegex[] =
    "passwords(?:-[a-z-]+\\.corp)?\\.google\\.com";

struct PasswordSiteUrlLazyInstanceTraits
    : public base::internal::DestructorAtExitLazyInstanceTraits<re2::RE2> {
  static re2::RE2* New(void* instance) {
    return CreateMatcher(instance, kPasswordSiteUrlRegex);
  }
};

base::LazyInstance<re2::RE2, PasswordSiteUrlLazyInstanceTraits>
    g_password_site_matcher = LAZY_INSTANCE_INITIALIZER;

}  // namespace

re2::RE2* CreateMatcher(void* instance, const char* pattern) {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  // Use placement new to initialize the instance in the preallocated space.
  // The "(instance)" is very important to force POD type initialization.
  re2::RE2* matcher = new (instance) re2::RE2(pattern, options);
  DCHECK(matcher->ok());
  return matcher;
}

bool IsGaiaReauthenticationForm(const blink::WebFormElement& form) {
  if (!gaia::HasGaiaSchemeHostPort(form.GetDocument().Url()))
    return false;

  bool has_rart_field = false;
  bool has_continue_field = false;

  for (const WebFormControlElement& element : form.GetFormControlElements()) {
    // We're only interested in the presence
    // of <input type="hidden" /> elements.
    const WebInputElement input = element.DynamicTo<WebInputElement>();
    if (!input || input.FormControlTypeForAutofill() !=
                      blink::mojom::FormControlType::kInputHidden) {
      continue;
    }

    // There must be a hidden input named "rart".
    if (input.FormControlName() == "rart")
      has_rart_field = true;

    // There must be a hidden input named "continue", whose value points
    // to a password (or password testing) site.
    if (input.FormControlName() == "continue" &&
        re2::RE2::PartialMatch(input.Value().Utf8(),
                               g_password_site_matcher.Get())) {
      has_continue_field = true;
    }
  }
  return has_rart_field && has_continue_field;
}

bool IsGaiaWithSkipSavePasswordForm(const blink::WebFormElement& form) {
  if (!gaia::HasGaiaSchemeHostPort(form.GetDocument().Url()))
    return false;

  GURL url(form.GetDocument().Url());
  std::string should_skip_password;
  if (!net::GetValueForKeyInQuery(url, "ssp", &should_skip_password))
    return false;
  return should_skip_password == "1";
}

std::string GetSignOnRealm(const GURL& origin) {
  GURL::Replacements rep;
  rep.SetPathStr("");
  return origin.ReplaceComponents(rep).spec();
}

}  // namespace autofill
