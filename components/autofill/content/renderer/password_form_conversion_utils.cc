// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
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
using form_util::UnownedFormElementsToFormData;
using form_util::WebFormElementToFormData;

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
    const std::vector<WebFormControlElement>& control_elements,
    const FormData& form_data,
    UsernameDetectorCache* username_detector_cache,
    const WebFormElement& form) {
  // Dummy cache stores the predictions in case no real cache was passed to
  // here.
  UsernameDetectorCache dummy_cache;
  if (!username_detector_cache)
    username_detector_cache = &dummy_cache;

  return GetPredictionsFieldBasedOnHtmlAttributes(
      control_elements, form_data, username_detector_cache, form);
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
    if (input.IsNull() || input.FormControlTypeForAutofill() !=
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

std::unique_ptr<FormData> CreateFormDataFromWebForm(
    const WebFormElement& web_form,
    const FieldDataManager* field_data_manager,
    UsernameDetectorCache* username_detector_cache,
    form_util::ButtonTitlesCache* button_titles_cache) {
  if (web_form.IsNull())
    return nullptr;

  auto form_data = std::make_unique<FormData>();
  form_data->is_gaia_with_skip_save_password_form =
      IsGaiaWithSkipSavePasswordForm(web_form) ||
      IsGaiaReauthenticationForm(web_form);

  blink::WebVector<WebFormControlElement> control_elements =
      web_form.GetFormControlElements();
  if (control_elements.empty())
    return nullptr;

  if (!WebFormElementToFormData(web_form, WebFormControlElement(),
                                field_data_manager, {ExtractOption::kValue},
                                form_data.get(), /*field=*/nullptr)) {
    return nullptr;
  }
  form_data->username_predictions =
      GetUsernamePredictions(control_elements.ReleaseVector(), *form_data,
                             username_detector_cache, web_form);
  form_data->button_titles =
      form_util::GetButtonTitles(web_form, button_titles_cache);

  return form_data;
}

std::unique_ptr<FormData> CreateFormDataFromUnownedInputElements(
    const WebLocalFrame& frame,
    const FieldDataManager* field_data_manager,
    UsernameDetectorCache* username_detector_cache,
    form_util::ButtonTitlesCache* button_titles_cache) {
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedFormFieldElements(frame.GetDocument());
  if (control_elements.empty())
    return nullptr;

  // Password manager does not merge forms across iframes and therefore does not
  // need to extract unowned iframes.
  std::vector<WebElement> iframe_elements;

  auto form_data = std::make_unique<FormData>();
  if (!UnownedFormElementsToFormData(control_elements, iframe_elements, nullptr,
                                     frame.GetDocument(), field_data_manager,
                                     {ExtractOption::kValue}, form_data.get(),
                                     /*field=*/nullptr)) {
    return nullptr;
  }

  form_data->username_predictions = GetUsernamePredictions(
      control_elements, *form_data, username_detector_cache, WebFormElement());

  return form_data;
}

std::string GetSignOnRealm(const GURL& origin) {
  GURL::Replacements rep;
  rep.SetPathStr("");
  return origin.ReplaceComponents(rep).spec();
}

}  // namespace autofill
