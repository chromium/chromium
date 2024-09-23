// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebString;

namespace autofill {

using form_util::ExtractOption;

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

// Extracts the username predictions. |control_elements| should be all the DOM
// elements of the form, |form_data| should be the already extracted FormData
// representation of that form. |username_detector_cache| is optional, and can
// be used to spare recomputation if called multiple times for the same form.
std::vector<FieldRendererId> GetUsernamePredictions(
    const FormData& form_data,
    UsernameDetectorCache* username_detector_cache) {
  // Dummy cache stores the predictions in case no real cache was passed to
  // here.
  UsernameDetectorCache dummy_cache;
  if (!username_detector_cache)
    username_detector_cache = &dummy_cache;

  return GetPredictionsFieldBasedOnHtmlAttributes(form_data,
                                                  username_detector_cache);
}

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

std::optional<FormData> CreateFormDataFromWebForm(
    const WebFormElement& web_form,
    const FieldDataManager& field_data_manager,
    UsernameDetectorCache* username_detector_cache,
    form_util::ButtonTitlesCache* button_titles_cache,
    const CallTimerState& timer_state) {
  if (!web_form) {
    return std::nullopt;
  }
  std::optional<FormData> form_data = form_util::ExtractFormData(
      web_form.GetDocument(), web_form, field_data_manager, timer_state);
  if (!form_data) {
    return std::nullopt;
  }
  form_data->set_is_gaia_with_skip_save_password_form(
      IsGaiaWithSkipSavePasswordForm(web_form) ||
      IsGaiaReauthenticationForm(web_form));

  form_data->set_username_predictions(
      GetUsernamePredictions(*form_data, username_detector_cache));
  form_data->set_button_titles(
      form_util::GetButtonTitles(web_form, button_titles_cache));
  return form_data;
}

std::optional<FormData> CreateFormDataFromUnownedInputElements(
    const WebLocalFrame& frame,
    const FieldDataManager& field_data_manager,
    UsernameDetectorCache* username_detector_cache,
    form_util::ButtonTitlesCache* button_titles_cache,
    const CallTimerState& timer_state) {
  std::optional<FormData> form_data = form_util::ExtractFormData(
      frame.GetDocument(), WebFormElement(), field_data_manager, timer_state);
  if (!form_data) {
    return std::nullopt;
  }
  form_data->set_username_predictions(
      GetUsernamePredictions(*form_data, username_detector_cache));
  return form_data;
}

std::string GetSignOnRealm(const GURL& origin) {
  GURL::Replacements rep;
  rep.SetPathStr("");
  return origin.ReplaceComponents(rep).spec();
}

}  // namespace autofill
