// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/prefilled_values_detector.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/content/renderer/suggestion_properties.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_util.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/renderer/render_frame.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebDocumentLoader;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;
using blink::WebView;

using password_manager::util::IsRendererRecognizedCredentialForm;

namespace autofill {

namespace {

using form_util::ExtractOption;
using form_util::GetFieldRendererId;
using form_util::GetFormByRendererId;
using form_util::GetFormControlByRendererId;
using form_util::GetFormRendererId;
using form_util::IsElementEditable;
using form_util::IsWebElementFocusableForAutofill;

using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

constexpr auto kInputPassword = blink::mojom::FormControlType::kInputPassword;

// The size above which we stop triggering autocomplete.
const size_t kMaximumTextSizeForAutocomplete = 1000;

// Names of HTML attributes to show form and field signatures for debugging.
const char kDebugAttributeForFormSignature[] = "form_signature";
const char kDebugAttributeForAlternativeFormSignature[] =
    "alternative_form_signature";
const char kDebugAttributeForFieldSignature[] = "field_signature";
const char kDebugAttributeForParserAnnotations[] = "pm_parser_annotation";
const char kDebugAttributeForVisibility[] = "visibility_annotation";
// Name of HTML attribute that stores the copy of autofill tooltip for
// debugging.
constexpr char kDebugAttributeForAutofill[] = "autofill-information";

// HTML attribute that is used as a tooltip if
// `kAutofillShowTypePredictions` is on.
constexpr char kHtmlAttributeForAutofillTooltip[] = "title";

// Maps element names to the actual elements to simplify form filling.
typedef std::map<std::u16string, WebInputElement> FormInputElementMap;

// Use the shorter name when referencing SavePasswordProgressLogger::StringID
// values to spare line breaks. The code provides enough context for that
// already.
typedef SavePasswordProgressLogger Logger;

typedef std::vector<FormInputElementMap> FormElementsList;

bool DoUsernamesMatch(const std::u16string& potential_suggestion,
                      const std::u16string& current_username,
                      bool exact_match) {
  if (potential_suggestion == current_username)
    return true;
  return !exact_match && IsPrefixOfEmailEndingWithAtSign(current_username,
                                                         potential_suggestion);
}

// Returns whether the `username_element` is allowed to be autofilled.
//
// Note that if the user interacts with the `password_field` and the
// `username_element` is user-defined (i.e., non-empty and non-autofilled), then
// this function returns false. This is a precaution, to not override the field
// if it has been classified as username by accident.
bool IsUsernameAmendable(const WebInputElement& username_element,
                         bool is_password_field_selected) {
  return username_element && IsElementEditable(username_element) &&
         (!is_password_field_selected || username_element.IsAutofilled() ||
          username_element.IsPreviewed() || username_element.Value().IsEmpty());
}

// Log `message` if `logger` is not null.
void LogMessage(Logger* logger, Logger::StringID message) {
  if (logger)
    logger->LogMessage(message);
}

// Log `message` and `value` if `logger` is not null.
void LogBoolean(Logger* logger, Logger::StringID message, bool value) {
  if (logger)
    logger->LogBoolean(message, value);
}

// Log a message including the name, method and action of |form|.
void LogHTMLForm(Logger* logger,
                 Logger::StringID message_id,
                 const WebFormElement& form) {
  if (logger) {
    logger->LogHTMLForm(message_id, form.GetName().Utf8(),
                        GURL(form.Action().Utf8()));
  }
}

// Returns true if there are any suggestions to be derived from `fill_data`.
// Only considers suggestions with usernames having `typed_username` as prefix.
bool CanShowUsernameSuggestion(const PasswordFormFillData& fill_data,
                               const std::u16string& typed_username) {
  std::u16string typed_username_lower = base::i18n::ToLower(typed_username);
  if (base::StartsWith(
          base::i18n::ToLower(fill_data.preferred_login.username_value),
          typed_username_lower, base::CompareCase::SENSITIVE)) {
    return true;
  }

  for (const auto& login : fill_data.additional_logins) {
    if (base::StartsWith(base::i18n::ToLower(login.username_value),
                         typed_username_lower, base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  return false;
}

// This function attempts to find the matching credentials for the
// `current_username` by scanning `fill_data`. The result is written in
// `username` and `password` parameters.
void FindMatchesByUsername(const PasswordFormFillData& fill_data,
                           const std::u16string& current_username,
                           bool exact_username_match,
                           RendererSavePasswordProgressLogger* logger,
                           std::u16string* username,
                           std::u16string* password) {
  // Look for any suitable matches to current field text.
  if (DoUsernamesMatch(fill_data.preferred_login.username_value,
                       current_username, exact_username_match)) {
    *username = fill_data.preferred_login.username_value;
    *password = fill_data.preferred_login.password_value;
    LogMessage(logger, Logger::STRING_USERNAMES_MATCH);
  } else {
    // Scan additional logins for a match.
    for (const auto& it : fill_data.additional_logins) {
      if (!it.realm.empty()) {
        // Non-empty realm means PSL match. Do not autofill PSL matched
        // credentials. The reason for this is that PSL matched sites are
        // different sites, so a password for a PSL matched site should be never
        // filled without explicit user selection.
        continue;
      }
      if (DoUsernamesMatch(it.username_value, current_username,
                           exact_username_match)) {
        *username = it.username_value;
        *password = it.password_value;
        break;
      }
    }
    LogBoolean(logger, Logger::STRING_MATCH_IN_ADDITIONAL,
               !(username->empty() && password->empty()));
  }
}

// TODO(crbug.com/40447274): This duplicates code from
// components/password_manager/core/browser/password_store/psl_matching_helper.h.
// The logic using this code should ultimately end up in
// components/password_manager/core/browser, at which point it can use the
// original code directly.
std::string GetRegistryControlledDomain(const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// TODO(crbug.com/40447274): This duplicates code from
// components/password_manager/core/browser/password_store/psl_matching_helper.h.
// The logic using this code should ultimately end up in
// components/password_manager/core/browser, at which point it can use the
// original code directly.
bool IsPublicSuffixDomainMatch(const std::string& url1,
                               const std::string& url2) {
  GURL gurl1(url1);
  GURL gurl2(url2);

  if (!gurl1.is_valid() || !gurl2.is_valid())
    return false;

  if (gurl1 == gurl2)
    return true;

  std::string domain1(GetRegistryControlledDomain(gurl1));
  std::string domain2(GetRegistryControlledDomain(gurl2));

  if (domain1.empty() || domain2.empty())
    return false;

  return gurl1.scheme() == gurl2.scheme() && domain1 == domain2 &&
         gurl1.port() == gurl2.port();
}

// Helper function that calculates form signature for `form_data` and returns it
// as a string.
std::string GetFormSignatureAsString(const FormData& form_data) {
  return base::NumberToString(CalculateFormSignature(form_data).value());
}
// Similar to `GetFormSignatureAsString` but returns alternative form signature
// as a string.
std::string GetAlternativeFormSignatureAsString(const FormData& form_data) {
  return base::NumberToString(
      CalculateAlternativeFormSignature(form_data).value());
}

// Sets the specified attribute of `target` to the given value. This must not
// happen while ScriptForbiddenScope is active (e.g. during
// blink::FrameLoader::FinishedParsing(), see crbug.com/1219852). Therefore,
// this function should be called asynchronously via SetAttributeAsync.
void SetAttributeInternal(WebElement target,
                          const std::string& attribute_utf8,
                          const std::string& value_utf8) {
  target.SetAttribute(WebString::FromUTF8(attribute_utf8),
                      WebString::FromUTF8(value_utf8));
}

// Posts an async task to call SetAttributeInternal.
void SetAttributeAsync(WebElement target,
                       const std::string& attribute_utf8,
                       const std::string& value_utf8) {
  if (!target) {
    return;
  }
  target.GetDocument()
      .GetFrame()
      ->GetTaskRunner(blink::TaskType::kInternalDefault)
      ->PostTask(FROM_HERE, base::BindOnce(&SetAttributeInternal, target,
                                           attribute_utf8, value_utf8));
}

// Annotate `fields` with field signatures, form signature and visibility state
// as HTML attributes.
void AnnotateFieldsWithSignatures(
    base::span<const WebFormControlElement> fields,
    const std::string& form_signature,
    const std::string& alternative_form_signature) {
  for (const WebFormControlElement& control_element : fields) {
    FieldSignature field_signature = CalculateFieldSignatureByNameAndType(
        control_element.NameForAutofill().Utf16(),
        form_util::ToAutofillFormControlType(
            control_element.FormControlTypeForAutofill()));
    SetAttributeAsync(control_element, kDebugAttributeForFieldSignature,
                      base::NumberToString(field_signature.value()));
    SetAttributeAsync(control_element, kDebugAttributeForFormSignature,
                      form_signature);
    SetAttributeAsync(control_element,
                      kDebugAttributeForAlternativeFormSignature,
                      alternative_form_signature);
    SetAttributeAsync(
        control_element, kDebugAttributeForVisibility,
        IsWebElementFocusableForAutofill(control_element) ? "true" : "false");
  }
}

// Returns true iff there is a password field in `frame`.
// We don't have to iterate through the whole DOM to find password fields.
// Instead, we can iterate through the fields of the forms and the unowned
// fields, both of which are cached in the Document.
bool HasPasswordField(const WebLocalFrame& frame) {
  auto ContainsPasswordField = [&](const auto& fields) {
    return base::Contains(fields, blink::mojom::FormControlType::kInputPassword,
                          &WebFormControlElement::FormControlTypeForAutofill);
  };

  WebDocument doc = frame.GetDocument();
  return std::ranges::any_of(doc.GetTopLevelForms(), ContainsPasswordField,
                             &WebFormElement::GetFormControlElements) ||
         ContainsPasswordField(doc.UnassociatedFormControls());
}

// Returns the closest visible autocompletable non-password text element
// preceding the `password_element` either in a form, if it belongs to one, or
// in the `frame`.
WebInputElement FindUsernameElementPrecedingPasswordElement(
    WebLocalFrame* frame,
    const WebInputElement& password_element) {
  DCHECK(password_element);

  std::vector<WebFormControlElement> elements =
      form_util::GetOwnedAutofillableFormControls(
          frame->GetDocument(), form_util::GetOwningForm(password_element));

  auto iter = base::ranges::find(elements, password_element);
  if (iter == elements.end())
    return WebInputElement();

  for (auto begin = elements.begin(); iter != begin;) {
    --iter;
    const WebInputElement input = iter->DynamicTo<WebInputElement>();
    if (input && input.IsTextField() &&
        input.FormControlTypeForAutofill() != kInputPassword &&
        IsElementEditable(input) && IsWebElementFocusableForAutofill(input)) {
      return input;
    }
  }

  return WebInputElement();
}

// Returns true if `element`'s frame origin is not PSL matched with the origin
// of any parent frame.
bool IsInCrossOriginIframeOrEmbeddedFrame(const WebInputElement& element) {
  WebFrame* cur_frame = element.GetDocument().GetFrame();
  WebString bottom_frame_origin = cur_frame->GetSecurityOrigin().ToString();

  DCHECK(cur_frame);

  while (cur_frame->Parent()) {
    cur_frame = cur_frame->Parent();
    if (!IsPublicSuffixDomainMatch(
            bottom_frame_origin.Utf8(),
            cur_frame->GetSecurityOrigin().ToString().Utf8())) {
      return true;
    }
  }
  // In MPArch, if we haven't reached the primary main frame, it means
  // we are in a nested frame tree. Fenced Frames are always considered
  // cross origin so we should return true here. Adding NOTREACHED for now
  // for future nested inner frame trees.
  if (!cur_frame->IsOutermostMainFrame()) {
    if (element.GetDocument().GetFrame()->IsInFencedFrameTree()) {
      return true;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
  return false;
}

void AnnotateFieldWithParsingResult(
    FieldRendererId renderer_id,
    const std::string& password_managers_annotation) {
  if (renderer_id.is_null())
    return;
  auto element = GetFormControlByRendererId(renderer_id);
  if (!element) {
    return;
  }
  // Calling SetAttribute synchronously here is safe because
  // AnnotateFieldWithParsingResult is triggered via a call from the the
  // browser. This means that we should not be in a ScriptForbiddenScope.
  element.SetAttribute(
      WebString::FromASCII(kDebugAttributeForParserAnnotations),
      WebString::FromASCII(password_managers_annotation));

  if (!base::FeatureList::IsEnabled(
          features::test::kAutofillShowTypePredictions)) {
    return;
  }

  if (!element.HasAttribute(kDebugAttributeForAutofill)) {
    // No autofill tooltip yet, don't fill anything.
    return;
  }

  std::string autofill_tooltip =
      element.GetAttribute(kDebugAttributeForAutofill).Utf8();

  element.SetAttribute(
      kHtmlAttributeForAutofillTooltip,
      WebString::FromUTF8(
          base::StrCat({element.GetAttribute(kDebugAttributeForAutofill).Utf8(),
                        "\n", kDebugAttributeForParserAnnotations, ": ",
                        password_managers_annotation})));
}

bool HasDocumentWithValidFrame(const WebInputElement& element) {
  WebFrame* frame = element.GetDocument().GetFrame();
  return frame && frame->View();
}

// This method tries to fix `fields` with empty typed or filled properties by
// matching them against previously filled or typed in fields with the same
// value and copying their filled or typed mask.
//
// This helps against websites where submitted fields differ from fields that
// had previously been autofilled or typed into.
[[nodiscard]] std::vector<FormFieldData> FillNonTypedOrFilledPropertiesMasks(
    std::vector<FormFieldData> fields,
    const FieldDataManager& manager) {
  static constexpr FieldPropertiesMask kFilledOrTyped =
      FieldPropertiesFlags::kAutofilled | FieldPropertiesFlags::kUserTyped;

  for (auto& field : fields) {
    if (field.properties_mask() & kFilledOrTyped) {
      continue;
    }

    for (const auto& [field_id, field_data] : manager.field_data_map()) {
      const std::optional<std::u16string>& value = field_data.first;
      FieldPropertiesMask properties = field_data.second;
      if ((properties & kFilledOrTyped) && value == field.value()) {
        field.set_properties_mask(field.properties_mask() |
                                  (properties & kFilledOrTyped));
        break;
      }
    }
  }
  return fields;
}

size_t GetIndexOfElement(const FormData& form_data,
                         const WebInputElement& element) {
  if (!element) {
    return form_data.fields().size();
  }
  for (size_t i = 0; i < form_data.fields().size(); ++i) {
    if (form_data.fields()[i].renderer_id() ==
        form_util::GetFieldRendererId(element)) {
      return i;
    }
  }
  return form_data.fields().size();
}

bool HasTextInputs(const FormData& form_data) {
  return std::ranges::any_of(form_data.fields(),
                             &FormFieldData::IsTextInputElement);
}

#if BUILDFLAG(IS_ANDROID)
bool IsWebAuthnForm(base::optional_ref<const FormData> form_data) {
  auto has_webauthn_attribute = [](const FormFieldData& field) {
    return field.parsed_autocomplete() && field.parsed_autocomplete()->webauthn;
  };
  return form_data &&
         std::ranges::any_of(form_data->fields(), has_webauthn_attribute);
}

// Returns a prediction whether the form that contains `username_element` and
// `password_element` will be ready for submission after filling these two
// elements.
// TODO(crbug.com/40248146): Consider to reduce `SubmissionReadinessState` to a
// boolean value (ready or not). The non-binary state is not needed for
// auto-submission (crbug.com/1283004), but showing TTF proactively
// (crbug.com/1393043) may need to check whether or not a given form comprises
// only two fields.
mojom::SubmissionReadinessState CalculateSubmissionReadiness(
    const FormData& form_data,
    const WebInputElement& username_element,
    WebInputElement password_element) {
  if (!password_element) {
    return mojom::SubmissionReadinessState::kNoPasswordField;
  }

  if (!username_element) {
    return mojom::SubmissionReadinessState::kNoUsernameField;
  }

  size_t username_index = GetIndexOfElement(form_data, username_element);
  size_t password_index = GetIndexOfElement(form_data, password_element);
  size_t number_of_elements = form_data.fields().size();
  if (username_index == number_of_elements ||
      password_index == number_of_elements) {
    // This is unexpected. `form_data` is supposed to contain username and
    // password elements.
    return mojom::SubmissionReadinessState::kError;
  }

  auto ShouldIgnoreField = [](const FormFieldData& field) {
    if (!field.IsFocusable())
      return true;
    // Don't treat a checkbox (e.g. "remember me") as an input field that may
    // block a form submission. Note: Don't use `check_status !=
    // kNotCheckable`, a radio button is considered a "checkable" element too,
    // but it should block a submission.
    return field.form_control_type() == mojom::FormControlType::kInputCheckbox;
  };

  for (size_t i = username_index + 1; i < password_index; ++i) {
    if (ShouldIgnoreField(form_data.fields()[i])) {
      continue;
    }
    return mojom::SubmissionReadinessState::kFieldBetweenUsernameAndPassword;
  }

  if (!password_element.IsLastInputElementInForm())
    return mojom::SubmissionReadinessState::kFieldAfterPasswordField;

  size_t number_of_visible_elements = 0;
  for (size_t i = 0; i < number_of_elements; ++i) {
    if (ShouldIgnoreField(form_data.fields()[i])) {
      continue;
    }

    if (username_index != i && password_index != i &&
        form_data.fields()[i].value().empty()) {
      return mojom::SubmissionReadinessState::kEmptyFields;
    }
    number_of_visible_elements++;
  }

  if (number_of_visible_elements > 2)
    return mojom::SubmissionReadinessState::kMoreThanTwoFields;

  return mojom::SubmissionReadinessState::kTwoFields;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

// During prerendering, we do not want the renderer to send messages to the
// corresponding driver. Since we use a channel associated interface, we still
// need to set up the mojo connection as before (i.e., we can't defer binding
// the interface). Instead, we enqueue our messages here as post-activation
// tasks. See post-prerendering activation steps here:
// https://wicg.github.io/nav-speculation/prerendering.html#prerendering-bcs-subsection
class PasswordAutofillAgent::DeferringPasswordManagerDriver
    : public mojom::PasswordManagerDriver {
 public:
  explicit DeferringPasswordManagerDriver(PasswordAutofillAgent* agent)
      : agent_(agent) {}
  ~DeferringPasswordManagerDriver() override = default;

 private:
  template <typename F, typename... Args>
  void SendMsg(F fn, Args&&... args) {
    DCHECK(!agent_->IsPrerendering());
    mojom::PasswordManagerDriver& password_manager_driver =
        agent_->GetPasswordManagerDriver();
    DCHECK_NE(&password_manager_driver, this);
    (password_manager_driver.*fn)(std::forward<Args>(args)...);
  }
  template <typename F, typename... Args>
  void DeferMsg(F fn, Args... args) {
    DCHECK(agent_->IsPrerendering());
    agent_->render_frame()
        ->GetWebFrame()
        ->GetDocument()
        .AddPostPrerenderingActivationStep(base::BindOnce(
            &DeferringPasswordManagerDriver::SendMsg<F, Args...>,
            weak_ptr_factory_.GetWeakPtr(), fn, std::forward<Args>(args)...));
  }
  void PasswordFormsParsed(const std::vector<FormData>& forms_data) override {
    DeferMsg(&mojom::PasswordManagerDriver::PasswordFormsParsed, forms_data);
  }
  void PasswordFormsRendered(
      const std::vector<FormData>& visible_forms_data) override {
    DeferMsg(&mojom::PasswordManagerDriver::PasswordFormsRendered,
             visible_forms_data);
  }
  void PasswordFormSubmitted(const FormData& form_data) override {
    DeferMsg(&mojom::PasswordManagerDriver::PasswordFormSubmitted, form_data);
  }
  void InformAboutUserInput(const FormData& form_data) override {
    DeferMsg(&mojom::PasswordManagerDriver::InformAboutUserInput, form_data);
  }
  void DynamicFormSubmission(
      mojom::SubmissionIndicatorEvent submission_indication_event) override {
    DeferMsg(&mojom::PasswordManagerDriver::DynamicFormSubmission,
             submission_indication_event);
  }
  void PasswordFormCleared(const FormData& form_data) override {
    DeferMsg(&mojom::PasswordManagerDriver::PasswordFormCleared, form_data);
  }
  void RecordSavePasswordProgress(const std::string& log) override {
    DeferMsg(&mojom::PasswordManagerDriver::RecordSavePasswordProgress, log);
  }
  void UserModifiedPasswordField() override {
    DeferMsg(&mojom::PasswordManagerDriver::UserModifiedPasswordField);
  }
  void UserModifiedNonPasswordField(FieldRendererId renderer_id,
                                    const std::u16string& value,
                                    bool autocomplete_attribute_has_username,
                                    bool is_likely_otp) override {
    DeferMsg(&mojom::PasswordManagerDriver::UserModifiedNonPasswordField,
             renderer_id, value, autocomplete_attribute_has_username,
             is_likely_otp);
  }
  void ShowPasswordSuggestions(
      const PasswordSuggestionRequest& request) override {
    DeferMsg(&mojom::PasswordManagerDriver::ShowPasswordSuggestions, request);
  }
#if BUILDFLAG(IS_ANDROID)
  void ShowKeyboardReplacingSurface(
      mojom::SubmissionReadinessState submission_readiness,
      bool is_webauthn_form) override {
    DeferMsg(&mojom::PasswordManagerDriver::ShowKeyboardReplacingSurface,
             submission_readiness, is_webauthn_form);
  }
#endif
  void CheckSafeBrowsingReputation(const GURL& form_action,
                                   const GURL& frame_url) override {
    DeferMsg(&mojom::PasswordManagerDriver::CheckSafeBrowsingReputation,
             form_action, frame_url);
  }
  void FocusedInputChanged(
      FieldRendererId focused_field_id,
      mojom::FocusedFieldType focused_field_type) override {
    DeferMsg(&mojom::PasswordManagerDriver::FocusedInputChanged,
             focused_field_id, focused_field_type);
  }
  void LogFirstFillingResult(FormRendererId form_renderer_id,
                             int32_t result) override {
    DeferMsg(&mojom::PasswordManagerDriver::LogFirstFillingResult,
             form_renderer_id, result);
  }

  raw_ptr<PasswordAutofillAgent> agent_ = nullptr;
  base::WeakPtrFactory<DeferringPasswordManagerDriver> weak_ptr_factory_{this};
};

PasswordAutofillAgent::FocusStateNotifier::FocusStateNotifier(
    PasswordAutofillAgent* agent)
    : agent_(CHECK_DEREF(agent)) {}

PasswordAutofillAgent::FocusStateNotifier::~FocusStateNotifier() = default;

void PasswordAutofillAgent::FocusStateNotifier::FocusedElementChanged(
    const WebElement& element) {
  mojom::FocusedFieldType new_focused_field_type =
      mojom::FocusedFieldType::kUnknown;
  FieldRendererId new_focused_field_id = FieldRendererId();
  if (auto form_control_element = element.DynamicTo<WebFormControlElement>()) {
    new_focused_field_type = GetFieldType(form_control_element);
    new_focused_field_id = form_util::GetFieldRendererId(form_control_element);
  }
  NotifyIfChanged(new_focused_field_type, new_focused_field_id);
}

mojom::FocusedFieldType PasswordAutofillAgent::FocusStateNotifier::GetFieldType(
    const WebFormControlElement& node) {
  auto form_control_type = node.FormControlTypeForAutofill();
  if (form_control_type == blink::mojom::FormControlType::kTextArea) {
    return mojom::FocusedFieldType::kFillableTextArea;
  }

  WebInputElement input_element = node.DynamicTo<WebInputElement>();
  if (!input_element || !input_element.IsTextField() ||
      !form_util::IsElementEditable(input_element)) {
    return mojom::FocusedFieldType::kUnfillableElement;
  }

  if (form_control_type == blink::mojom::FormControlType::kInputSearch) {
    return mojom::FocusedFieldType::kFillableSearchField;
  }
  if (form_control_type == blink::mojom::FormControlType::kInputPassword) {
    return mojom::FocusedFieldType::kFillablePasswordField;
  }
  if (agent_->IsUsernameInputField(input_element)) {
    return mojom::FocusedFieldType::kFillableUsernameField;
  }
  if (form_util::IsWebauthnTaggedElement(node)) {
    return mojom::FocusedFieldType::kFillableWebauthnTaggedField;
  }
  return mojom::FocusedFieldType::kFillableNonSearchField;
}

void PasswordAutofillAgent::FocusStateNotifier::NotifyIfChanged(
    mojom::FocusedFieldType new_focused_field_type,
    FieldRendererId new_focused_field_id) {
  // Forward the request if the focused field is different from the previous
  // one.
  if (focused_field_id_ == new_focused_field_id &&
      focused_field_type_ == new_focused_field_type) {
    return;
  }

  agent_->GetPasswordManagerDriver().FocusedInputChanged(
      new_focused_field_id, new_focused_field_type);

  focused_field_type_ = new_focused_field_type;
  focused_field_id_ = new_focused_field_id;
}

PasswordAutofillAgent::PasswordAutofillAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry,
    EnableHeavyFormDataScraping enable_heavy_form_data_scraping)
    : content::RenderFrameObserver(render_frame),
      enable_heavy_form_data_scraping_(enable_heavy_form_data_scraping) {
  registry->AddInterface<mojom::PasswordAutofillAgent>(base::BindRepeating(
      &PasswordAutofillAgent::BindPendingReceiver, base::Unretained(this)));
}

PasswordAutofillAgent::~PasswordAutofillAgent() = default;

void PasswordAutofillAgent::Init(AutofillAgent* autofill_agent) {
  autofill_agent_ = autofill_agent;
}

void PasswordAutofillAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::PasswordAutofillAgent>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void PasswordAutofillAgent::SetPasswordGenerationAgent(
    PasswordGenerationAgent* generation_agent) {
  password_generation_agent_ = generation_agent;
}

PasswordAutofillAgent::FormStructureInfo::FormStructureInfo() = default;

PasswordAutofillAgent::FormStructureInfo::FormStructureInfo(
    const FormStructureInfo& other) = default;

PasswordAutofillAgent::FormStructureInfo&
PasswordAutofillAgent::FormStructureInfo::operator=(
    const PasswordAutofillAgent::FormStructureInfo& other) = default;

PasswordAutofillAgent::FormStructureInfo::FormStructureInfo(
    FormStructureInfo&& other) = default;

PasswordAutofillAgent::FormStructureInfo&
PasswordAutofillAgent::FormStructureInfo::operator=(
    PasswordAutofillAgent::FormStructureInfo&& other) = default;

PasswordAutofillAgent::FormStructureInfo::~FormStructureInfo() = default;

PasswordAutofillAgent::PasswordValueGatekeeper::PasswordValueGatekeeper()
    : was_user_gesture_seen_(false) {}

PasswordAutofillAgent::PasswordValueGatekeeper::~PasswordValueGatekeeper() =
    default;

void PasswordAutofillAgent::PasswordValueGatekeeper::RegisterElement(
    WebInputElement element) {
  CHECK(element);
  if (was_user_gesture_seen_) {
    ShowValue(element);
  } else {
    elements_.emplace_back(element);
  }
}

void PasswordAutofillAgent::PasswordValueGatekeeper::OnUserGesture() {
  if (was_user_gesture_seen_) {
    return;
  }
  was_user_gesture_seen_ = true;
  for (FieldRef element : elements_) {
    if (WebInputElement input_element =
            element.GetField().DynamicTo<WebInputElement>()) {
      ShowValue(input_element);
    }
  }
  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::Reset() {
  was_user_gesture_seen_ = false;
  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::ShowValue(
    WebInputElement element) {
  if (element && !element.SuggestedValue().IsEmpty()) {
    element.SetAutofillValue(element.SuggestedValue());
  }
}

bool PasswordAutofillAgent::TextDidChangeInTextField(
    const WebInputElement& element) {
  CHECK(element);
  // Show the popup with the list of available usernames.
  return ShowSuggestions(element,
                         AutofillSuggestionTriggerSource::kTextFieldDidChange);
}

// LINT.IfChange

void PasswordAutofillAgent::NotifyPasswordManagerAboutFieldModification(
    const WebInputElement& element) {
  if (element.FormControlTypeForAutofill() == kInputPassword) {
    auto iter = password_to_username_.find(FieldRef(element));
    if (iter != password_to_username_.end()) {
      // Note that the suggested value of `mutable_element` was reset when its
      // value changed.
      // TODO(crbug.com/41132785): Do this through const WebInputElement.
      WebInputElement mutable_element = element;  // We need a non-const.
      mutable_element.SetAutofillState(WebAutofillState::kNotFilled);
    }
    GetPasswordManagerDriver().UserModifiedPasswordField();
    return;
  }

  const std::u16string element_value = element.Value().Utf16();
  static base::NoDestructor<WebString> kAutocomplete("autocomplete");
  std::string autocomplete_attribute =
      element.GetAttribute(*kAutocomplete).Utf8();
  static base::NoDestructor<WebString> kName("name");
  std::u16string name_attribute = element.GetAttribute(*kName).Utf16();
  std::u16string id_attribute = element.GetIdAttribute().Utf16();
  static base::NoDestructor<WebString> kLabel("label");
  std::u16string label_attribute = element.GetAttribute(*kLabel).Utf16();

  if (!password_manager::util::CanFieldBeConsideredAsSingleUsername(
          name_attribute, id_attribute, label_attribute) ||
      !password_manager::util::CanValueBeConsideredAsSingleUsername(
          element_value)) {
    return;
  }

  bool is_likely_otp = password_manager::util::IsLikelyOtp(
      name_attribute, id_attribute, autocomplete_attribute);

  GetPasswordManagerDriver().UserModifiedNonPasswordField(
      GetFieldRendererId(element), element_value,
      base::Contains(autocomplete_attribute,
                     password_manager::constants::kAutocompleteUsername),
      is_likely_otp);
}

void PasswordAutofillAgent::UpdatePasswordStateForTextChange(
    const WebInputElement& element) {
  NotifyPasswordManagerAboutFieldModification(element);

  InformBrowserAboutUserInput(form_util::GetOwningForm(element), element);
}

// LINT.ThenChange(//components/password_manager/core/browser/password_manager.cc:update_password_state_for_text_change)

void PasswordAutofillAgent::TrackAutofilledElement(
    const WebFormControlElement& element) {
  autofill_agent_->TrackAutofilledElement(element);
}

void PasswordAutofillAgent::FillPasswordSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  auto focused_element = last_queried_element().DynamicTo<WebInputElement>();
  if (!focused_element || !IsElementEditable(focused_element)) {
    return;
  }
  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;
  if (!HasElementsToFill(focused_element, UseFallbackData(true),
                         &username_element, &password_element,
                         &password_info)) {
    return;
  }
  if (focused_element.FormControlTypeForAutofill() == kInputPassword) {
    CHECK(password_element);
    password_info->password_field_suggestion_was_accepted = true;
    password_info->password_field = FieldRef(password_element);
  }
  FillUsernameAndPasswordElements(username_element, password_element, username,
                                  password);
}

void PasswordAutofillAgent::FillPasswordSuggestionById(
    FieldRendererId username_element_id,
    FieldRendererId password_element_id,
    const std::u16string& username,
    const std::u16string& password) {
  if (!last_queried_element()) {
    return;
  }
  FillUsernameAndPasswordElements(
      GetFormControlByRendererId(username_element_id)
          .DynamicTo<WebInputElement>(),
      GetFormControlByRendererId(password_element_id)
          .DynamicTo<WebInputElement>(),
      username, password);
}

void PasswordAutofillAgent::FillUsernameAndPasswordElements(
    blink::WebInputElement username_element,
    blink::WebInputElement password_element,
    const std::u16string& username,
    const std::u16string& password) {
  ClearPreviewedForm();
  WebFormControlElement focused_element = last_queried_element();
  // TODO(crbug.com/341995827): Remove dependency on `focused_element`. Username
  // filling condition could be made similar to the password one and selection
  // range setting could be skipped.
  CHECK(focused_element);
  // Call OnFieldAutofilled before WebInputElement::SetAutofillState which may
  // cause frame closing.
  if (password_element && password_generation_agent_) {
    password_generation_agent_->OnFieldAutofilled(password_element);
  }
  bool is_password_field_focused = password_element == focused_element;
  if (IsUsernameAmendable(username_element, is_password_field_focused) &&
      !(username.empty() && is_password_field_focused) &&
      username_element.Value().Utf16() != username) {
    DoFillField(username_element, username);
  }
  if (password_element && IsElementEditable(password_element)) {
    FillPasswordFieldAndSave(password_element, password);
    // TODO(crbug.com/40223173): As Touch-To-Fill and auto-submission don't
    // currently support filling single username fields, the code below is
    // within `password_element`. Support such fields too and move the
    // code out the condition.
    // If the `username_element` is visible/focusable and the `password_element`
    // is not, trigger submission on the former as the latter unlikely has an
    // Enter listener.
    if (username_element &&
        IsWebElementFocusableForAutofill(username_element) &&
        !IsWebElementFocusableForAutofill(password_element)) {
      field_renderer_id_to_submit_ = GetFieldRendererId(username_element);
    } else {
      field_renderer_id_to_submit_ = GetFieldRendererId(password_element);
    }
  }
  auto length = base::checked_cast<unsigned>(focused_element.Value().length());
  focused_element.SetSelectionRange(length, length);
}

void PasswordAutofillAgent::FillIntoFocusedField(
    bool is_password,
    const std::u16string& credential) {
  auto focused_input = last_queried_element().DynamicTo<WebInputElement>();
  if (!focused_input || focused_input.IsReadOnly()) {
    return;
  }
  if (!is_password) {
    DoFillField(focused_input, credential);
  }
  if (focused_input.FormControlTypeForAutofill() != kInputPassword) {
    return;
  }
  FillPasswordFieldAndSave(focused_input, credential);
}

void PasswordAutofillAgent::PreviewField(FieldRendererId field_id,
                                         const std::u16string& value) {
  WebInputElement input = form_util::GetFormControlByRendererId(field_id)
                              .DynamicTo<WebInputElement>();
  if (!input || !IsElementEditable(input)) {
    return;
  }
  DoPreviewField(
      input, value,
      /*is_password=*/input.FormControlTypeForAutofill() == kInputPassword);
}

void PasswordAutofillAgent::FillField(FieldRendererId field_id,
                                      const std::u16string& value) {
  WebFormControlElement form_control =
      form_util::GetFormControlByRendererId(field_id);
  WebInputElement input_element = form_control.DynamicTo<WebInputElement>();
  if (!input_element || input_element.IsReadOnly()) {
    // Early return for non-input fields such as textarea.
    return;
  }
  DoFillField(input_element, value);
}

void PasswordAutofillAgent::DoPreviewField(WebInputElement input,
                                           const std::u16string& credential,
                                           bool is_password) {
  CHECK(input);
  previewed_elements_.emplace_back(PreviewInfo{
      .field_id = form_util::GetFieldRendererId(input),
      .autofill_state = input.GetAutofillState(),
      .is_password = is_password,
  });
  input.SetSuggestedValue(WebString::FromUTF16(credential));
}

void PasswordAutofillAgent::DoFillField(WebInputElement input,
                                        const std::u16string& credential) {
  CHECK(input);
  input.SetAutofillValue(WebString::FromUTF16(credential));
  field_data_manager().UpdateFieldDataMap(
      form_util::GetFieldRendererId(input), credential,
      FieldPropertiesFlags::kAutofilledOnUserTrigger);
  TrackAutofilledElement(input);
}

void PasswordAutofillAgent::FillPasswordFieldAndSave(
    WebInputElement password_input,
    const std::u16string& credential) {
  CHECK(password_input.FormControlTypeForAutofill() == kInputPassword);
  DoFillField(password_input, credential);
  InformBrowserAboutUserInput(form_util::GetOwningForm(password_input),
                              password_input);
}

void PasswordAutofillAgent::PreviewSuggestion(
    const WebFormControlElement& control_element,
    const std::u16string& username,
    const std::u16string& password) {
  // The element in context of the suggestion popup.
  const WebInputElement element = control_element.DynamicTo<WebInputElement>();
  if (!element || !IsElementEditable(element)) {
    return;
  }
  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info;
  if (!HasElementsToFill(element, UseFallbackData(true), &username_element,
                         &password_element, &password_info)) {
    return;
  }
  PreviewUsernameAndPasswordElements(username_element, password_element,
                                     username, password);
}

void PasswordAutofillAgent::PreviewPasswordSuggestionById(
    FieldRendererId username_element_id,
    FieldRendererId password_element_id,
    const std::u16string& username,
    const std::u16string& password) {
  if (!last_queried_element()) {
    return;
  }
  PreviewUsernameAndPasswordElements(
      GetFormControlByRendererId(username_element_id)
          .DynamicTo<WebInputElement>(),
      GetFormControlByRendererId(password_element_id)
          .DynamicTo<WebInputElement>(),
      username, password);
}

void PasswordAutofillAgent::PreviewUsernameAndPasswordElements(
    blink::WebInputElement username_element,
    blink::WebInputElement password_element,
    const std::u16string& username,
    const std::u16string& password) {
  WebFormControlElement focused_element = last_queried_element();
  // TODO(crbug.com/341995827): Remove dependency on `focused_element` when
  // similar dependency is removed from
  // `PasswordAutofillAgent::FillUsernameAndPasswordElements`.
  CHECK(focused_element);
  if (IsUsernameAmendable(username_element,
                          password_element == focused_element)) {
    DoPreviewField(username_element, username, /*is_password=*/false);
  }
  if (password_element && IsElementEditable(password_element)) {
    DoPreviewField(password_element, password, /*is_password=*/true);
  }
}

void PasswordAutofillAgent::ClearPreviewedForm() {
  for (const PreviewInfo& preview_info : previewed_elements_) {
    WebInputElement element =
        form_util::GetFormControlByRendererId(preview_info.field_id)
            .DynamicTo<WebInputElement>();
    if (!element) {
      continue;
    }
    element.SetSuggestedValue(WebString());
    element.SetAutofillState(preview_info.autofill_state);
  }
  previewed_elements_.clear();
}

bool PasswordAutofillAgent::FindPasswordInfoForElement(
    const WebInputElement& element,
    UseFallbackData use_fallback_data,
    WebInputElement* username_element,
    WebInputElement* password_element,
    PasswordInfo** password_info) {
  DCHECK(username_element && password_element && password_info);
  username_element->Reset();
  password_element->Reset();
  if (!element) {
    return false;
  }
  if (suggestion_banned_fields_.contains(GetFieldRendererId(element))) {
    // No suggestion for `element` since there is a reliable signal that
    // `element` is a non-credential field.
    return false;
  }
  if (element.FormControlTypeForAutofill() != kInputPassword) {
    *username_element = element;
  } else {
    *password_element = element;

    // If there is a password field, but a request to the store hasn't been sent
    // yet, then do fetch saved credentials now.
    if (!sent_request_to_store_) {
      SendPasswordForms(false);
      return false;
    }

    auto iter = web_input_to_password_info_.find(FieldRef(element));
    if (iter == web_input_to_password_info_.end()) {
      auto password_iter = password_to_username_.find(FieldRef(element));
      if (password_iter == password_to_username_.end()) {
        if (!use_fallback_data || web_input_to_password_info_.empty()) {
          return false;
        }
        iter = last_supplied_password_info_iter_;
      } else {
        *username_element =
            password_iter->second.GetField().DynamicTo<WebInputElement>();
      }
    }

    if (iter != web_input_to_password_info_.end()) {
      // It's a password field without corresponding username field. Try to find
      // the username field based on visibility.
      *username_element = FindUsernameElementPrecedingPasswordElement(
          render_frame()->GetWebFrame(), *password_element);
      *password_info = &iter->second;
      return true;
    }
  }

  if (username_element == nullptr || username_element->IsNull()) {
    return false;
  }
  auto iter = web_input_to_password_info_.find(FieldRef(*username_element));
  if (iter == web_input_to_password_info_.end()) {
    return false;
  }
  *password_info = &iter->second;
  if (password_element->IsNull()) {
    if (WebInputElement password_input = (*password_info)
                                             ->password_field.GetField()
                                             .DynamicTo<WebInputElement>()) {
      *password_element = password_input;
    }
  }
  return true;
}

bool PasswordAutofillAgent::IsUsernameOrPasswordFillable(
    const WebInputElement& username_element,
    const WebInputElement& password_element,
    PasswordInfo* password_info) {
  if (username_element && IsElementEditable(username_element) &&
      !password_element) {
    return true;
  }
  if (password_element && IsElementEditable(password_element)) {
    return true;
  }
  // Password field might be disabled before entering valid username.
  // For such cases, check that the password field is under <form> tag.
  // Otherwise, username and password element might not be related.
  return username_element && IsElementEditable(username_element) &&
         (password_info != nullptr &&
          !password_info->fill_data.form_renderer_id.is_null());
}

bool PasswordAutofillAgent::HasElementsToFill(
    const WebInputElement& trigger_element,
    UseFallbackData use_fallback_data,
    WebInputElement* username_element,
    WebInputElement* password_element,
    PasswordInfo** password_info) {
  if (!FindPasswordInfoForElement(trigger_element, use_fallback_data,
                                  username_element, password_element,
                                  password_info)) {
    return false;
  }
  return IsUsernameOrPasswordFillable(*username_element, *password_element,
                                      *password_info);
}

void PasswordAutofillAgent::MaybeCheckSafeBrowsingReputation(
    const WebInputElement& element) {
  // Enabled on desktop and Android
#if BUILDFLAG(FULL_SAFE_BROWSING) || BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  // Note: A site may use a Password field to collect a CVV or a Credit Card
  // number, but showing a slightly misleading warning here is better than
  // showing no warning at all.
  if (element.FormControlTypeForAutofill() != kInputPassword) {
    return;
  }
  if (checked_safe_browsing_reputation_)
    return;

  checked_safe_browsing_reputation_ = true;
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  GURL frame_url = GURL(frame->GetDocument().Url());
  WebFormElement form_element = form_util::GetOwningForm(element);
  GURL action_url = form_element
                        ? form_util::GetCanonicalActionForForm(form_element)
                        : GURL();
  GetPasswordManagerDriver().CheckSafeBrowsingReputation(action_url, frame_url);
#endif
}

#if BUILDFLAG(IS_ANDROID)
bool PasswordAutofillAgent::ShouldSuppressKeyboard() {
  // The keyboard should be suppressed if a keyboard replacing surface is
  // displayed (e.g. TouchToFill).
  return keyboard_replacing_surface_state_ ==
         KeyboardReplacingSurfaceState::kIsShowing;
}

bool PasswordAutofillAgent::TryToShowKeyboardReplacingSurface(
    const WebFormControlElement& control_element) {
  if (keyboard_replacing_surface_state_ !=
      KeyboardReplacingSurfaceState::kShouldShow) {
    return false;
  }

  const WebInputElement input_element =
      control_element.DynamicTo<WebInputElement>();
  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;
  if (!input_element || !IsElementEditable(input_element) ||
      !FindPasswordInfoForElement(input_element, UseFallbackData(false),
                                  &username_element, &password_element,
                                  &password_info)) {
    return false;
  }

  bool has_amendable_username_element = IsUsernameAmendable(
      username_element,
      input_element.FormControlTypeForAutofill() == kInputPassword);
  bool has_editable_password_element =
      password_element && IsElementEditable(password_element);
  CHECK(has_amendable_username_element || has_editable_password_element);

  WebFormElement form = password_element
                            ? form_util::GetOwningForm(password_element)
                            : form_util::GetOwningForm(username_element);
  std::optional<FormData> form_data =
      form ? GetFormDataFromWebForm(form)
           : GetFormDataFromUnownedInputElements();

  GetPasswordManagerDriver().ShowKeyboardReplacingSurface(
      form_data ? CalculateSubmissionReadiness(*form_data, username_element,
                                               password_element)
                : mojom::SubmissionReadinessState::kNoInformation,
      IsWebAuthnForm(form_data));

  keyboard_replacing_surface_state_ = KeyboardReplacingSurfaceState::kIsShowing;
  return true;
}
#endif

bool PasswordAutofillAgent::ShowSuggestions(
    const WebInputElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  return trigger_source ==
                 AutofillSuggestionTriggerSource::kManualFallbackPasswords
             ? ShowManualFallbackSuggestions(element)
             : ShowSuggestionsForDomain(element, trigger_source);
}

bool PasswordAutofillAgent::FrameCanAccessPasswordManager() {
  // about:blank or about:srcdoc frames should not be allowed to use password
  // manager.  See https://crbug.com/756587.
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  WebURL url = frame->GetDocument().Url();
  if (!url.ProtocolIs(url::kHttpScheme) && !url.ProtocolIs(url::kHttpsScheme))
    return false;
  return frame->GetSecurityOrigin().CanAccessPasswordManager();
}

void PasswordAutofillAgent::OnDynamicFormsSeen() {
  SendPasswordForms(false /* only_visible */);
}

void PasswordAutofillAgent::UserGestureObserved() {
  autofilled_elements_cache_.clear();

  gatekeeper_.OnUserGesture();
}

void PasswordAutofillAgent::AnnotateFormsAndFieldsWithSignatures(
    WebVector<WebFormElement>& forms) {
  if (!render_frame()) {
    return;
  }
  WebDocument document = render_frame()->GetWebFrame()->GetDocument();
  for (const WebFormElement& form : forms) {
    std::optional<FormData> form_data = GetFormDataFromWebForm(form);
    std::string form_signature;
    std::string alternative_form_signature;
    if (form_data) {
      // GetAlternativeFormSignatureAsString() require the FormData::url.
      form_data->set_url(document.Url());
      form_signature = GetFormSignatureAsString(*form_data);
      alternative_form_signature =
          GetAlternativeFormSignatureAsString(*form_data);
      SetAttributeAsync(form, kDebugAttributeForFormSignature, form_signature);
      SetAttributeAsync(form, kDebugAttributeForAlternativeFormSignature,
                        alternative_form_signature);
    }
    AnnotateFieldsWithSignatures(
        form_util::GetOwnedAutofillableFormControls(document, form),
        form_signature, alternative_form_signature);
  }

  std::optional<FormData> form_data = GetFormDataFromUnownedInputElements();
  std::string form_signature;
  std::string alternative_form_signature;
  if (form_data) {
    // GetFormSignatureAsString() may require the FormData::url.
    form_data->set_url(render_frame()->GetWebFrame()->GetDocument().Url());
    form_signature = GetFormSignatureAsString(*form_data);
    alternative_form_signature =
        GetAlternativeFormSignatureAsString(*form_data);
  }
  AnnotateFieldsWithSignatures(
      form_util::GetOwnedAutofillableFormControls(document, WebFormElement()),
      form_signature, alternative_form_signature);
}

void PasswordAutofillAgent::SendPasswordForms(bool only_visible) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger = std::make_unique<RendererSavePasswordProgressLogger>(
        &GetPasswordManagerDriver());
    logger->LogMessage(Logger::STRING_SEND_PASSWORD_FORMS_METHOD);
    logger->LogBoolean(Logger::STRING_ONLY_VISIBLE, only_visible);
  }

  WebLocalFrame* frame = render_frame()->GetWebFrame();

  // Make sure that this security origin is allowed to use password manager.
  WebDocument doc = frame->GetDocument();
  WebSecurityOrigin origin = doc.GetSecurityOrigin();
  if (logger) {
    logger->LogURL(Logger::STRING_SECURITY_ORIGIN,
                   GURL(origin.ToString().Utf8()));
  }
  if (!FrameCanAccessPasswordManager()) {
    LogMessage(logger.get(), Logger::STRING_SECURITY_ORIGIN_FAILURE);
    return;
  }

  // Checks whether the webpage is a redirect page or an empty page.
  if (form_util::IsWebpageEmpty(frame)) {
    LogMessage(logger.get(), Logger::STRING_WEBPAGE_EMPTY);
    return;
  }

  WebVector<WebFormElement> forms = doc.GetTopLevelForms();

  if (IsShowAutofillSignaturesEnabled())
    AnnotateFormsAndFieldsWithSignatures(forms);
  if (logger)
    logger->LogNumber(Logger::STRING_NUMBER_OF_ALL_FORMS, forms.size());

  std::vector<FormData> password_forms_data;
  for (const WebFormElement& form : forms) {
    if (only_visible) {
      bool is_form_visible = std::ranges::any_of(
          form.GetFormControlElements(), &IsWebElementFocusableForAutofill);
      LogHTMLForm(logger.get(), Logger::STRING_FORM_FOUND_ON_PAGE, form);
      LogBoolean(logger.get(), Logger::STRING_FORM_IS_VISIBLE, is_form_visible);

      // If requested, ignore non-rendered forms, e.g., those styled with
      // display:none.
      if (!is_form_visible)
        continue;
    }

    std::optional<FormData> form_data(GetFormDataFromWebForm(form));
    if (!form_data || !IsRendererRecognizedCredentialForm(*form_data)) {
      continue;
    }

    FormStructureInfo form_structure_info =
        ExtractFormStructureInfo(*form_data);
    if (only_visible || WasFormStructureChanged(form_structure_info)) {
      forms_structure_cache_[form_structure_info.renderer_id] =
          std::move(form_structure_info);

      password_forms_data.push_back(std::move(*form_data));
      continue;
    }

    std::vector<WebFormControlElement> control_elements =
        form.GetFormControlElements().ReleaseVector();
    // Sometimes JS can change autofilled forms. In this case we try to restore
    // values for the changed elements.
    TryFixAutofilledForm(control_elements);
  }

  // See if there are any unassociated input elements that could be used for
  // password submission.
  // TODO(crbug.com/41422255): Consider using TryFixAutofilledForm for the cases
  // when there is no form tag.
  bool add_unowned_inputs = true;
  if (only_visible) {
    std::vector<WebFormControlElement> control_elements =
        form_util::GetOwnedAutofillableFormControls(doc, WebFormElement());
    add_unowned_inputs = std::ranges::any_of(control_elements,
                                             &IsWebElementFocusableForAutofill);
    LogBoolean(logger.get(), Logger::STRING_UNOWNED_INPUTS_VISIBLE,
               add_unowned_inputs);
  }

  if (add_unowned_inputs) {
    std::optional<FormData> form_data(GetFormDataFromUnownedInputElements());
    if (form_data && IsRendererRecognizedCredentialForm(*form_data)) {
      password_forms_data.push_back(std::move(*form_data));
    }
  }

  if (only_visible) {
    // Send the PasswordFormsRendered message regardless of whether
    // `password_forms_data` is empty. The empty `password_forms_data` are a
    // possible signal to the browser that a pending login attempt succeeded.
    GetPasswordManagerDriver().PasswordFormsRendered(password_forms_data);
  } else {
    // If there is a password field, but the list of password forms is empty for
    // some reason, add a dummy form to the list. It will cause a request to the
    // store. Therefore, saved passwords will be available for filling on click.
    if (!sent_request_to_store_ && password_forms_data.empty() &&
        HasPasswordField(*frame)) {
      // Set everything that `FormDigest` needs.
      password_forms_data.emplace_back();
    }
    if (!password_forms_data.empty()) {
      sent_request_to_store_ = true;
      GetPasswordManagerDriver().PasswordFormsParsed(password_forms_data);
    }
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Provide warnings about the accessibility of password forms on the page.
  if (!password_forms_data.empty() &&
      (doc.Url().ProtocolIs(url::kHttpScheme) ||
       doc.Url().ProtocolIs(url::kHttpsScheme))) {
    page_passwords_analyser_.AnalyseDocumentDOM(frame);
  }
#endif
}

void PasswordAutofillAgent::DidDispatchDOMContentLoadedEvent() {
  SendPasswordForms(false);
}

void PasswordAutofillAgent::DidFinishLoad() {
  // The `frame` contents have been rendered.  Let the PasswordManager know
  // which of the loaded frames are actually visible to the user.  This also
  // triggers the "Save password?" infobar if the user just submitted a password
  // form.
  SendPasswordForms(true);
}

void PasswordAutofillAgent::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  checked_safe_browsing_reputation_ = false;
  recorded_first_filling_result_ = false;
}

void PasswordAutofillAgent::OnDestruct() {
  receiver_.reset();
}

bool PasswordAutofillAgent::IsPrerendering() const {
  return render_frame()->GetWebFrame()->GetDocument().IsPrerendering();
}

bool PasswordAutofillAgent::IsUsernameInputField(
    const WebInputElement& input_element) const {
  return input_element &&
         input_element.FormControlTypeForAutofill() != kInputPassword &&
         base::Contains(web_input_to_password_info_, FieldRef(input_element));
}

void PasswordAutofillAgent::ReadyToCommitNavigation(
    WebDocumentLoader* document_loader) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger = std::make_unique<RendererSavePasswordProgressLogger>(
        &GetPasswordManagerDriver());
    logger->LogMessage(Logger::STRING_DID_START_PROVISIONAL_LOAD_METHOD);
  }

  WebLocalFrame* navigated_frame = render_frame()->GetWebFrame();
  if (navigated_frame->IsOutermostMainFrame()) {
    // This is a new navigation, so require a new user gesture before filling in
    // passwords.
    gatekeeper_.Reset();
  } else {
    LogMessage(logger.get(), Logger::STRING_FRAME_NOT_MAIN_FRAME);
  }

  CleanupOnDocumentShutdown();
}

void PasswordAutofillAgent::OnProbablyFormSubmitted() {}

// mojom::PasswordAutofillAgent:
void PasswordAutofillAgent::SetPasswordFillData(
    const PasswordFormFillData& form_data) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger = std::make_unique<RendererSavePasswordProgressLogger>(
        &GetPasswordManagerDriver());
    logger->LogMessage(Logger::STRING_ON_FILL_PASSWORD_FORM_METHOD);
  }
  suggestion_banned_fields_ = form_data.suggestion_banned_fields;

  bool username_password_fields_not_set =
      form_data.username_element_renderer_id.is_null() &&
      form_data.password_element_renderer_id.is_null();
  if (username_password_fields_not_set) {
    // No fields for filling were found during parsing, which means filling
    // fallback case. So save data for fallback filling.
    MaybeStoreFallbackData(form_data);
    return;
  }

  WebInputElement username_element, password_element;
  std::tie(username_element, password_element) =
      FindUsernamePasswordElements(form_data);
  bool is_single_username_fill =
      form_data.password_element_renderer_id.is_null();
  WebElement main_element =
      is_single_username_fill ? username_element : password_element;
  if (!main_element) {
    MaybeStoreFallbackData(form_data);
    // TODO(crbug.com/40626063): Fix logging for single username.
    LogFirstFillingResult(form_data, FillingResult::kNoPasswordElement);
    return;
  }

  times_received_fill_data_[form_data.form_renderer_id]++;
  StoreDataForFillOnAccountSelect(form_data, username_element,
                                  password_element);

  // If wait_for_username is true, we don't want to initially fill the form
  // until the user types in a valid username.
  if (form_data.wait_for_username) {
    LogFirstFillingResult(form_data, FillingResult::kWaitForUsername);
    MaybeTriggerSuggestionsOnFocusedElement(username_element, password_element);
    return;
  }

  FillUserNameAndPassword(username_element, password_element, form_data,
                          logger.get());
}

void PasswordAutofillAgent::SetLoggingState(bool active) {
  logging_state_active_ = active;
}

void PasswordAutofillAgent::AnnotateFieldsWithParsingResult(
    const ParsingResult& parsing_result) {
  AnnotateFieldWithParsingResult(parsing_result.username_renderer_id,
                                 "username_element");
  AnnotateFieldWithParsingResult(parsing_result.password_renderer_id,
                                 "password_element");
  AnnotateFieldWithParsingResult(parsing_result.new_password_renderer_id,
                                 "new_password_element");
  AnnotateFieldWithParsingResult(parsing_result.confirm_password_renderer_id,
                                 "confirmation_password_element");
}

void PasswordAutofillAgent::InformNoSavedCredentials(
    bool should_show_popup_without_passwords) {
  should_show_popup_without_passwords_ = should_show_popup_without_passwords;

  autofilled_elements_cache_.clear();

  // Clear the actual field values.
  std::vector<WebFormControlElement> elements;
  elements.reserve(all_autofilled_elements_.size());
  for (FieldRendererId id : all_autofilled_elements_) {
    elements.push_back(form_util::GetFormControlByRendererId(id));
  }

  for (WebFormControlElement element : elements) {
    if (!element) {
      continue;
    }
    // Don't clear the actual value of fields that the user has edited manually
    // (which changes the autofill state back to kNotFilled).
    if (element.IsAutofilled()) {
      element.SetValue(WebString());
    }
    element.SetSuggestedValue(WebString());
  }
  all_autofilled_elements_.clear();

  field_data_manager().ClearData();
}

#if BUILDFLAG(IS_ANDROID)
void PasswordAutofillAgent::KeyboardReplacingSurfaceClosed(
    bool show_virtual_keyboard) {
  keyboard_replacing_surface_state_ = KeyboardReplacingSurfaceState::kWasShown;

  auto focused_input = last_queried_element().DynamicTo<WebInputElement>();
  if (!focused_input || focused_input.IsReadOnly()) {
    return;
  }

  if (show_virtual_keyboard) {
    render_frame()->ShowVirtualKeyboard();

    // Since a keyboard replacing surface suppresses the Autofill popup,
    // re-trigger the suggestions in case the virtual keyboard should be shown.
    // This is limited to the keyboard accessory, as otherwise it would result
    // in a flickering of the popup, due to showing the keyboard at the same
    // time.
    ShowSuggestions(
        focused_input,
        autofill::AutofillSuggestionTriggerSource::kFormControlElementClicked);
  }
}

void PasswordAutofillAgent::TriggerFormSubmission() {
  // Find the last interacted element to simulate an enter keystroke at.
  WebFormControlElement form_control =
      GetFormControlByRendererId(field_renderer_id_to_submit_);
  if (!form_control) {
    // The target field doesn't exist anymore. Don't try to submit it.
    return;
  }

  // `form_control` can only be `WebInputElement`, not `WebSelectElement`.
  WebInputElement input = form_control.To<WebInputElement>();
  input.DispatchSimulatedEnter();
  field_renderer_id_to_submit_ = FieldRendererId();
}
#endif

std::optional<FormData> PasswordAutofillAgent::GetFormDataFromWebForm(
    const WebFormElement& web_form) {
  return CreateFormDataFromWebForm(
      web_form, field_data_manager(), &username_detector_cache_,
      &button_titles_cache_,
      autofill_agent_->GetCallTimerState(
          CallTimerState::CallSite::kGetFormDataFromWebForm));
}

std::optional<FormData>
PasswordAutofillAgent::GetFormDataFromUnownedInputElements() {
  // The element's frame might have been detached in the meantime (see
  // http://crbug.com/585363, comments 5 and 6), in which case `frame` will
  // be null. This was hardly caused by form submission (unless the user is
  // supernaturally quick), so it is OK to drop the ball here.
  content::RenderFrame* frame = render_frame();
  if (!frame)
    return std::nullopt;
  WebLocalFrame* web_frame = frame->GetWebFrame();
  if (!web_frame)
    return std::nullopt;
  return CreateFormDataFromUnownedInputElements(
      *web_frame, field_data_manager(), &username_detector_cache_,
      *enable_heavy_form_data_scraping_ ? &button_titles_cache_ : nullptr,
      autofill_agent_->GetCallTimerState(
          CallTimerState::CallSite::kGetFormDataFromUnownedInputElements));
}

void PasswordAutofillAgent::InformAboutFormClearing(
    const WebFormElement& form) {
  if (!FrameCanAccessPasswordManager())
    return;
  for (const auto& element : form.GetFormControlElements()) {
    // Notify PasswordManager if `form` has password fields that have user typed
    // input or input autofilled on user trigger.
    if (IsPasswordFieldFilledByUser(element)) {
      NotifyPasswordManagerAboutClearedForm(form);
      return;
    }
  }
}

void PasswordAutofillAgent::InformAboutFieldClearing(
    const WebInputElement& cleared_element) {
  if (!FrameCanAccessPasswordManager())
    return;
  DCHECK(cleared_element.Value().IsEmpty());
  FieldRendererId field_id = form_util::GetFieldRendererId(cleared_element);
  // Ignore fields that had no user input or autofill on user trigger.
  if (!field_data_manager().DidUserType(field_id) &&
      !field_data_manager().WasAutofilledOnUserTrigger(field_id)) {
    return;
  }

  WebFormElement form = form_util::GetOwningForm(cleared_element);
  if (!form) {
    // Process password field clearing for fields outside the <form> tag.
    if (auto unowned_form_data = GetFormDataFromUnownedInputElements())
      GetPasswordManagerDriver().PasswordFormCleared(*unowned_form_data);
    return;
  }
  // Process field clearing for a form under a <form> tag.
  // Only notify PasswordManager in case all user filled password fields were
  // cleared.
  bool cleared_all_password_fields = std::ranges::all_of(
      form.GetFormControlElements(), [this](const auto& el) {
        return !IsPasswordFieldFilledByUser(el) || el.Value().IsEmpty();
      });
  if (cleared_all_password_fields)
    NotifyPasswordManagerAboutClearedForm(form);
}

bool PasswordAutofillAgent::ShowSuggestionsForDomain(
    const WebInputElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;
  FindPasswordInfoForElement(element, UseFallbackData(true), &username_element,
                             &password_element, &password_info);

  if (!password_info) {
    MaybeCheckSafeBrowsingReputation(element);
    if (!CanShowPopupWithoutPasswords(password_element)) {
      return false;
    }
  }

  if (!element.IsTextField() || !IsElementEditable(element)) {
    return false;
  }
  // Check that at least one fillable element is editable.
  if (!IsUsernameOrPasswordFillable(username_element, password_element,
                                    password_info)) {
    return false;
  }

  // Don't attempt to autofill with values that are too large.
  if (element.Value().length() > kMaximumTextSizeForAutocomplete) {
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  // Don't call `ShowSuggestionPopup` if a keyboard replacing surface is
  // currently showing. Since a keyboard replacing surface in spirit is very
  // similar to a suggestion pop-up, return true so that the AutofillAgent does
  // not try to show other autofill suggestions instead.
  if (keyboard_replacing_surface_state_ ==
      KeyboardReplacingSurfaceState::kIsShowing) {
    return true;
  }
#endif

  if (!HasDocumentWithValidFrame(element)) {
    return false;
  }

  // If a username element is focused, show suggestions unless all possible
  // usernames are filtered.
  if (element.FormControlTypeForAutofill() != kInputPassword) {
    std::u16string username_prefix;
    if (!ShouldShowFullSuggestionListForPasswordManager(trigger_source,
                                                        element) &&
        !base::FeatureList::IsEnabled(
            password_manager::features::kNoPasswordSuggestionFiltering)) {
      if (!password_info ||
          !CanShowUsernameSuggestion(password_info->fill_data,
                                     element.Value().Utf16())) {
        return false;
      }
      username_prefix = element.Value().Utf16();
    }
    ShowSuggestionPopup(username_prefix, element, trigger_source);
    return true;
  }

  // If the element is a password field, do not to show a popup if the user has
  // already accepted a password suggestion on another password field.
  if (password_info && password_info->password_field_suggestion_was_accepted &&
      element != password_info->password_field.GetField()) {
    return true;
  }

  // Show suggestions for password fields only while they are empty.
  if (!element.IsAutofilled() && !element.Value().IsEmpty()) {
    HidePopup();
    return false;
  }

  ShowSuggestionPopup(std::u16string(), element, trigger_source);
  return true;
}

bool PasswordAutofillAgent::ShowManualFallbackSuggestions(
    const WebInputElement& element) {
  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;
  if (!FindPasswordInfoForElement(element, UseFallbackData(false),
                                  &username_element, &password_element,
                                  &password_info)) {
    // Perform this action only if there's no passwords saved for the triggering
    // field. Manual fallback suggestions can be shown on any field.
    MaybeCheckSafeBrowsingReputation(element);
  }

  if (!FrameCanAccessPasswordManager()) {
    return false;
  }

  if (!HasDocumentWithValidFrame(element)) {
    return false;
  }

  ShowSuggestionPopup(std::u16string(), element,
                      AutofillSuggestionTriggerSource::kManualFallbackPasswords);
  return true;
}

void PasswordAutofillAgent::ShowSuggestionPopup(
    const std::u16string& typed_username,
    const WebInputElement& user_input,
    AutofillSuggestionTriggerSource trigger_source) {
  base::UmaHistogramEnumeration("PasswordManager.SuggestionPopupTriggerSource",
                                trigger_source);
  FormData form;
  FormFieldData field;
  if (std::optional<std::pair<FormData, raw_ref<const FormFieldData>>>
          form_and_field = form_util::FindFormAndFieldForFormControlElement(
              user_input, field_data_manager(),
              autofill_agent_->GetCallTimerState(
                  CallTimerState::CallSite::kShowSuggestionPopup),
              /*extract_options=*/{})) {
    form = std::move(form_and_field->first);
    field = *form_and_field->second;
  }

  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;
  FindPasswordInfoForElement(user_input, UseFallbackData(false),
                             &username_element, &password_element,
                             &password_info);
  size_t username_element_index = GetIndexOfElement(form, username_element);
  if (!username_element || !IsElementEditable(username_element)) {
    username_element_index = form.fields().size();
  }
  size_t password_element_index = GetIndexOfElement(form, password_element);
  if (!password_element || !IsElementEditable(password_element)) {
    password_element_index = form.fields().size();
  }

  const bool show_webauthn_credentials =
      field.parsed_autocomplete() && field.parsed_autocomplete()->webauthn;
  GetPasswordManagerDriver().ShowPasswordSuggestions(PasswordSuggestionRequest(
      field.renderer_id(), form, trigger_source, username_element_index,
      password_element_index, field.text_direction(), typed_username,
      show_webauthn_credentials,
      gfx::RectF(render_frame()->ConvertViewportToWindow(
          user_input.BoundsInWidget()))));
}

void PasswordAutofillAgent::CleanupOnDocumentShutdown() {
  web_input_to_password_info_.clear();
  password_to_username_.clear();
  last_supplied_password_info_iter_ = web_input_to_password_info_.end();
  should_show_popup_without_passwords_ = false;
  field_data_manager().ClearData();
  previewed_elements_.clear();
  sent_request_to_store_ = false;
  checked_safe_browsing_reputation_ = false;
  username_detector_cache_.clear();
  forms_structure_cache_.clear();
  autofilled_elements_cache_.clear();
  all_autofilled_elements_.clear();
  field_renderer_id_to_submit_ = FieldRendererId();
  suggestion_banned_fields_.clear();
#if BUILDFLAG(IS_ANDROID)
  keyboard_replacing_surface_state_ =
      KeyboardReplacingSurfaceState::kShouldShow;
#endif
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  page_passwords_analyser_.Reset();
#endif

  for (const auto& [_, times_received_data] : times_received_fill_data_) {
    base::UmaHistogramCounts100("PasswordManager.TimesReceivedFillDataForForm",
                                times_received_data);
  }
  times_received_fill_data_.clear();
}

void PasswordAutofillAgent::InformBrowserAboutUserInput(
    const WebFormElement& form,
    const WebInputElement& element) {
  DCHECK(form || element);
  if (!FrameCanAccessPasswordManager())
    return;
  std::optional<FormData> form_data =
      form ? GetFormDataFromWebForm(form)
           : GetFormDataFromUnownedInputElements();
  if (!form_data)
    return;

  // Notify the browser about user inputs if `form` is recognized as a
  // credential form in the renderer, or if the browser has parsed `element` as
  // password-related and provided filling data for it.
  if (IsRendererRecognizedCredentialForm(*form_data) ||
      (element && web_input_to_password_info_.contains(FieldRef(element)))) {
    GetPasswordManagerDriver().InformAboutUserInput(*form_data);
  }
}

bool PasswordAutofillAgent::FillUserNameAndPassword(
    WebInputElement username_element,
    WebInputElement password_element,
    const PasswordFormFillData& fill_data,
    RendererSavePasswordProgressLogger* logger) {
  LogMessage(logger, Logger::STRING_FILL_USERNAME_AND_PASSWORD_METHOD);

  bool is_single_username_fill = !password_element;
  WebInputElement main_element =
      is_single_username_fill ? username_element : password_element;

  if (IsInCrossOriginIframeOrEmbeddedFrame(main_element)) {
    LogMessage(logger, Logger::STRING_FAILED_TO_FILL_INTO_IFRAME);
    LogFirstFillingResult(fill_data, FillingResult::kBlockedByFrameHierarchy);
    return false;
  }

  // Don't fill username if password can't be set.
  if (!IsElementEditable(main_element)) {
    LogMessage(logger,
               Logger::STRING_FAILED_TO_FILL_NO_AUTOCOMPLETEABLE_ELEMENT);
    LogFirstFillingResult(fill_data,
                          FillingResult::kPasswordElementIsNotAutocompleteable);
    return false;
  }

  // `current_username` is the username for credentials that are going to be
  // autofilled. It is selected according to the algorithm:
  // 1. If the page already contains a non-empty value in `username_element`
  // that is not found in the list of values known to be used as placeholders,
  // this is adopted and not overridden.
  // 2. Default username from `fill_data` if the username field is
  // autocompletable.
  // 3. Empty if username field doesn't exist or if username field is empty and
  // not autocompletable (no username case).
  std::u16string current_username;

  // Whether the username element was prefilled with content that was on a
  // list of known placeholder texts that should be overridden (e.g. "username
  // or email" or there is a server hint that it is just a placeholder).
  bool prefilled_placeholder_username = false;

  if (username_element) {
    // This is a heuristic guess. If the credential is stored for
    // www.example.com, the username may be prefilled with "@example.com".
    std::string possible_email_domain =
        GetRegistryControlledDomain(fill_data.url);

    prefilled_placeholder_username =
        !username_element.Value().IsEmpty() &&
        (PossiblePrefilledUsernameValue(username_element.Value().Utf8(),
                                        possible_email_domain) ||
         (fill_data.username_may_use_prefilled_placeholder &&
          base::FeatureList::IsEnabled(
              password_manager::features::
                  kEnableOverwritingPlaceholderUsernames)));

    if (!username_element.Value().IsEmpty() &&
        username_element.GetAutofillState() == WebAutofillState::kNotFilled &&
        !prefilled_placeholder_username) {
      // Username is filled with content that was not on a list of known
      // placeholder texts (e.g. "username or email") nor there is server-side
      // data that this value is placeholder.
      current_username = username_element.Value().Utf16();
    } else if (IsElementEditable(username_element)) {
      current_username = fill_data.preferred_login.username_value;
    }
  }

  // `username` and `password` will contain the match found if any.
  std::u16string username;
  std::u16string password;

  bool exact_username_match =
      !username_element || IsElementEditable(username_element);

  FindMatchesByUsername(fill_data, current_username, exact_username_match,
                        logger, &username, &password);

  if (password.empty() && !is_single_username_fill) {
    if (username_element && !username_element.Value().IsEmpty() &&
        !prefilled_placeholder_username) {
      LogPrefilledUsernameFillOutcome(
          PrefilledUsernameFillOutcome::kPrefilledUsernameNotOverridden);

      LogMessage(logger, Logger::STRING_FAILED_TO_FILL_PREFILLED_USERNAME);
      LogFirstFillingResult(
          fill_data, FillingResult::kUsernamePrefilledWithIncompatibleValue);
      return false;
    }
    LogMessage(logger,
               Logger::STRING_FAILED_TO_FILL_FOUND_NO_PASSWORD_FOR_USERNAME);
    LogFirstFillingResult(fill_data,
                          FillingResult::kFoundNoPasswordForUsername);
    return false;
  }

  // Call OnFieldAutofilled before WebInputElement::SetAutofillState which may
  // cause frame closing.
  if (password_generation_agent_ && !is_single_username_fill)
    password_generation_agent_->OnFieldAutofilled(password_element);

  // Input matches the username, fill in required values.
  if (username_element && IsElementEditable(username_element)) {
    if (!username.empty() &&
        (username_element.Value().IsEmpty() ||
         username_element.GetAutofillState() != WebAutofillState::kNotFilled ||
         prefilled_placeholder_username)) {
      AutofillField(username, username_element);
      if (prefilled_placeholder_username) {
        LogPrefilledUsernameFillOutcome(
            PrefilledUsernameFillOutcome::
                kPrefilledPlaceholderUsernameOverridden);
      }
    }
    if (logger)
      logger->LogElementName(Logger::STRING_USERNAME_FILLED, username_element);
  }

  if (!is_single_username_fill) {
    AutofillField(password, password_element);
    if (logger)
      logger->LogElementName(Logger::STRING_PASSWORD_FILLED, password_element);
  }

  LogFirstFillingResult(fill_data, FillingResult::kSuccess);
  return true;
}

void PasswordAutofillAgent::LogPrefilledUsernameFillOutcome(
    PrefilledUsernameFillOutcome outcome) {
  if (prefilled_username_metrics_logged_)
    return;
  prefilled_username_metrics_logged_ = true;
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.PrefilledUsernameFillOutcome",
                            outcome);
}

void PasswordAutofillAgent::OnProvisionallySaveForm(
    const WebFormElement& form,
    const WebFormControlElement& element,
    SaveFormReason source) {
  // SaveFormReason::kTextFieldChanged is handled in
  // AutofillAgent::OnTextFieldDidChange(). For the sake of code clarity, please
  // don't add handling for SaveFormReason::kTextFieldChanged here if
  // possible.
  if (source == SaveFormReason::kWillSendSubmitEvent) {
    WebInputElement input_element = element.DynamicTo<WebInputElement>();
    InformBrowserAboutUserInput(form, input_element);
  }
}

void PasswordAutofillAgent::FireHostSubmitEvent(
    FormRendererId form_id,
    mojom::SubmissionSource source) {
  switch (source) {
    case mojom::SubmissionSource::NONE:
      NOTREACHED();
    case mojom::SubmissionSource::FORM_SUBMISSION:
      OnFormSubmitted(GetFormByRendererId(form_id));
      return;
    case mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return;
    case mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
    case mojom::SubmissionSource::XHR_SUCCEEDED:
    case mojom::SubmissionSource::FRAME_DETACHED:
    case mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      if (FrameCanAccessPasswordManager()) {
        GetPasswordManagerDriver().DynamicFormSubmission(
            ToSubmissionIndicatorEvent(source));
      }
      return;
  }
  NOTREACHED();
}

void PasswordAutofillAgent::OnFormSubmitted(const WebFormElement& form) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger = std::make_unique<RendererSavePasswordProgressLogger>(
        &GetPasswordManagerDriver());
    LogHTMLForm(logger.get(), Logger::STRING_HTML_FORM_FOR_SUBMIT, form);
  }

  if (!FrameCanAccessPasswordManager()) {
    LogMessage(logger.get(), Logger::STRING_SECURITY_ORIGIN_FAILURE);
    return;
  }

  std::optional<FormData> submitted_form_data = GetFormDataFromWebForm(form);

  if (!submitted_form_data || !HasTextInputs(*submitted_form_data)) {
    return;
  }

  submitted_form_data->set_submission_event(
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);

  submitted_form_data->set_fields(FillNonTypedOrFilledPropertiesMasks(
      submitted_form_data->ExtractFields(), field_data_manager()));

  GetPasswordManagerDriver().PasswordFormSubmitted(*submitted_form_data);
}

void PasswordAutofillAgent::OnInferredFormSubmission(SubmissionSource source) {
  switch (source) {
    case mojom::SubmissionSource::NONE:
    case mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED:
    case mojom::SubmissionSource::FORM_SUBMISSION:
      NOTREACHED();
    case mojom::SubmissionSource::FRAME_DETACHED:
      // If a sub frame has been destroyed while the user was entering
      // information into a password form, try to save the data. See
      // https://crbug.com/450806 or examples of sites that perform login using
      // this technique. We are treating primary main frame and the root of
      // embedded frames the same on purpose.
      if (FrameCanAccessPasswordManager() &&
          render_frame()->GetWebFrame()->Parent()) {
        GetPasswordManagerDriver().DynamicFormSubmission(
            SubmissionIndicatorEvent::FRAME_DETACHED);
      }
      CleanupOnDocumentShutdown();
      return;
    case mojom::SubmissionSource::SAME_DOCUMENT_NAVIGATION:
    case mojom::SubmissionSource::XHR_SUCCEEDED:
    case mojom::SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      if (FrameCanAccessPasswordManager()) {
        GetPasswordManagerDriver().DynamicFormSubmission(
            ToSubmissionIndicatorEvent(source));
      }
      return;
  }
}

void PasswordAutofillAgent::HidePopup() {
  if (autofill_agent_->unsafe_autofill_driver()) {
    autofill_agent_->unsafe_autofill_driver()->HidePopup();
  }
}

mojom::PasswordManagerDriver&
PasswordAutofillAgent::GetPasswordManagerDriver() {
  if (IsPrerendering()) {
    if (!deferring_password_manager_driver_) {
      deferring_password_manager_driver_ =
          std::make_unique<DeferringPasswordManagerDriver>(this);
    }
    return *deferring_password_manager_driver_;
  }

  // Lazily bind this interface.
  if (!password_manager_driver_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &password_manager_driver_);
  }

  return *password_manager_driver_;
}

std::pair<WebInputElement, WebInputElement>
PasswordAutofillAgent::FindUsernamePasswordElements(
    const PasswordFormFillData& form_data) {
  const FieldRendererId username_renderer_id =
      form_data.username_element_renderer_id;
  const FieldRendererId password_renderer_id =
      form_data.password_element_renderer_id;
  const bool is_username_present = !username_renderer_id.is_null();
  const bool is_password_present = !password_renderer_id.is_null();

  std::vector<FieldRendererId> element_ids;
  if (is_password_present)
    element_ids.push_back(password_renderer_id);
  if (is_username_present)
    element_ids.push_back(username_renderer_id);

  std::vector<WebFormControlElement> elements;
  elements.reserve(element_ids.size());
  for (FieldRendererId id : element_ids) {
    elements.push_back(form_util::GetFormControlByRendererId(id));
  }

  // Set password element.
  WebInputElement password_field;
  size_t current_index = 0;
  if (is_password_present)
    password_field = elements[current_index++].DynamicTo<WebInputElement>();

  // Set username element.
  WebInputElement username_field;
  if (is_username_present)
    username_field = elements[current_index++].DynamicTo<WebInputElement>();

  return std::make_pair(username_field, password_field);
}

void PasswordAutofillAgent::StoreDataForFillOnAccountSelect(
    const PasswordFormFillData& form_data,
    WebInputElement username_element,
    WebInputElement password_element) {
  WebInputElement main_element =
      username_element ? username_element : password_element;

  PasswordInfo password_info;
  password_info.fill_data = form_data;
  if (password_element) {
    password_info.password_field = FieldRef(password_element);
  }
  if (main_element) {
    web_input_to_password_info_[FieldRef(main_element)] = password_info;
    last_supplied_password_info_iter_ =
        web_input_to_password_info_.find(FieldRef(main_element));
    if (main_element.FormControlTypeForAutofill() != kInputPassword &&
        password_element && username_element) {
      password_to_username_[FieldRef(password_element)] =
          FieldRef(username_element);
    }
  }
}

void PasswordAutofillAgent::MaybeStoreFallbackData(
    const PasswordFormFillData& form_data) {
  if (!web_input_to_password_info_.empty())
    return;
  // If for some reasons elements for filling were not found (for example
  // because they were renamed by JavaScript) then add fill data for
  // `web_input_to_password_info_`. When the user clicks on a password field
  // which is not a key in `web_input_to_password_info_`, the first element from
  // `web_input_to_password_info_` will be used in
  // PasswordAutofillAgent::FindPasswordInfoForElement to propose to fill.
  PasswordInfo password_info;
  password_info.fill_data = form_data;
  web_input_to_password_info_[FieldRef()] = password_info;
  last_supplied_password_info_iter_ = web_input_to_password_info_.begin();
}

void PasswordAutofillAgent::LogFirstFillingResult(
    const PasswordFormFillData& form_data,
    FillingResult result) {
  if (recorded_first_filling_result_)
    return;
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.FirstRendererFillingResult",
                            result);
  GetPasswordManagerDriver().LogFirstFillingResult(
      form_data.form_renderer_id, base::strict_cast<int32_t>(result));
  recorded_first_filling_result_ = true;
}

PasswordAutofillAgent::FormStructureInfo
PasswordAutofillAgent::ExtractFormStructureInfo(const FormData& form_data) {
  FormStructureInfo result;
  result.renderer_id = form_data.renderer_id();
  result.fields.resize(form_data.fields().size());

  for (size_t i = 0; i < form_data.fields().size(); ++i) {
    const FormFieldData& form_field = form_data.fields()[i];

    FormFieldInfo& field_info = result.fields[i];
    field_info.renderer_id = form_field.renderer_id();
    field_info.form_control_type = form_field.form_control_type();
    field_info.autocomplete_attribute = form_field.autocomplete_attribute();
    field_info.is_focusable = form_field.is_focusable();
  }

  return result;
}

bool PasswordAutofillAgent::WasFormStructureChanged(
    const FormStructureInfo& form_info) const {
  if (form_info.renderer_id.is_null()) {
    return true;
  }

  auto cached_form = forms_structure_cache_.find(form_info.renderer_id);
  if (cached_form == forms_structure_cache_.end())
    return true;

  const FormStructureInfo& cached_form_info = cached_form->second;

  if (form_info.fields.size() != cached_form_info.fields.size())
    return true;

  for (size_t i = 0; i < form_info.fields.size(); ++i) {
    const FormFieldInfo& form_field = form_info.fields[i];
    const FormFieldInfo& cached_form_field = cached_form_info.fields[i];

    if (form_field.renderer_id != cached_form_field.renderer_id) {
      return true;
    }

    if (form_field.form_control_type != cached_form_field.form_control_type)
      return true;

    if (form_field.autocomplete_attribute !=
        cached_form_field.autocomplete_attribute) {
      return true;
    }

    if (form_field.is_focusable != cached_form_field.is_focusable)
      return true;
  }
  return false;
}

void PasswordAutofillAgent::TryFixAutofilledForm(
    std::vector<WebFormControlElement>& control_elements) const {
  for (WebFormControlElement& control_element : control_elements) {
    auto cached_element = autofilled_elements_cache_.find(
        form_util::GetFieldRendererId(control_element));
    if (cached_element == autofilled_elements_cache_.end()) {
      continue;
    }
    // autofilled_elements_cache_ stores values filled at page load time and
    // gets wiped when we observe a user gesture. During this time, the
    // username/password fields can be in preview state and we restore this
    // state if JavaScript modifies the field's value.
    const WebString& cached_value = cached_element->second;
    if (cached_value != control_element.SuggestedValue()) {
      control_element.SetSuggestedValue(cached_value);
    }
  }
}

void PasswordAutofillAgent::AutofillField(const std::u16string& value,
                                          WebInputElement field) {
  CHECK(field);
  // Do not autofill on load fields that have any user typed input.
  const FieldRendererId field_id = form_util::GetFieldRendererId(field);
  if (field_data_manager().DidUserType(field_id)) {
    return;
  }
  if (field.Value().Utf16() == value) {
    return;
  }
  field.SetSuggestedValue(WebString::FromUTF16(value));
  field.SetAutofillState(WebAutofillState::kAutofilled);
  // Wait to fill until a user gesture occurs. This is to make sure that we do
  // not fill in the DOM with a password until we believe the user is
  // intentionally interacting with the page.
  gatekeeper_.RegisterElement(field);
  field_data_manager().UpdateFieldDataMap(
      field_id, value, FieldPropertiesFlags::kAutofilledOnPageLoad);
  autofilled_elements_cache_.emplace(field_id, WebString::FromUTF16(value));
  all_autofilled_elements_.insert(field_id);
}

bool PasswordAutofillAgent::CanShowPopupWithoutPasswords(
    const WebInputElement& password_element) const {
  return should_show_popup_without_passwords_ && password_element &&
         IsElementEditable(password_element);
}

bool PasswordAutofillAgent::IsPasswordFieldFilledByUser(
    const WebFormControlElement& element) const {
  FieldRendererId element_id = form_util::GetFieldRendererId(element);
  return element.FormControlTypeForAutofill() ==
             blink::mojom::FormControlType::kInputPassword &&
         (field_data_manager().DidUserType(element_id) ||
          field_data_manager().WasAutofilledOnUserTrigger(element_id));
}

void PasswordAutofillAgent::NotifyPasswordManagerAboutClearedForm(
    const WebFormElement& cleared_form) {
  WebDocument document = render_frame()
                             ? render_frame()->GetWebFrame()->GetDocument()
                             : WebDocument();
  if (std::optional<FormData> form_data = form_util::ExtractFormData(
          document, cleared_form, field_data_manager(),
          autofill_agent_->GetCallTimerState(
              CallTimerState::CallSite::
                  kNotifyPasswordManagerAboutClearedForm))) {
    GetPasswordManagerDriver().PasswordFormCleared(*form_data);
  }
}

void PasswordAutofillAgent::MaybeTriggerSuggestionsOnFocusedElement(
    const WebInputElement& username_element,
    const WebInputElement& password_element) {
  WebInputElement focused_element;
  if (username_element && username_element.Focused()) {
    focused_element = username_element;
  } else if (password_element && password_element.Focused()) {
    focused_element = password_element;
  } else {
    return;
  }

  auto form_data =
      GetFormDataFromWebForm(form_util::GetOwningForm(focused_element));
  if (form_data && (times_received_fill_data_[form_data->renderer_id()] == 1) &&
#if BUILDFLAG(IS_ANDROID)
      // Limit showing suggestions on autofocus to WebAuthn forms only, since
      // Android suggestion UI (TTF) can be much more intrusive.
      IsWebAuthnForm(form_data) &&
#endif  // BUILDFLAG(IS_ANDROID)
      base::FeatureList::IsEnabled(
          password_manager::features::kShowSuggestionsOnAutofocus)) {
    autofill_agent_->TriggerSuggestions(
        autofill::form_util::GetFieldRendererId(focused_element),
        AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField);
  }
}

}  // namespace autofill
