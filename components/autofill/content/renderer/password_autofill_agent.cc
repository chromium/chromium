// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/linked_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/security_state/core/security_state.h"
#include "content/public/common/origin_util.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

using blink::ToWebInputElement;
using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormElement;
using blink::WebFormControlElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebString;
using blink::WebVector;
using blink::WebView;

namespace autofill {
namespace {

// The size above which we stop triggering autocomplete.
const size_t kMaximumTextSizeForAutocomplete = 1000;

const char kDummyUsernameField[] = "anonymous_username";
const char kDummyPasswordField[] = "anonymous_password";

// Names of HTML attributes to show form and field signatures for debugging.
const char kDebugAttributeForFormSignature[] = "form_signature";
const char kDebugAttributeForFieldSignature[] = "field_signature";
const char kDebugAttributeForParserAnnotations[] = "pm_parser_annotation";

// Maps element names to the actual elements to simplify form filling.
typedef std::map<base::string16, WebInputElement> FormInputElementMap;

// Use the shorter name when referencing SavePasswordProgressLogger::StringID
// values to spare line breaks. The code provides enough context for that
// already.
typedef SavePasswordProgressLogger Logger;

typedef std::vector<FormInputElementMap> FormElementsList;

bool FillDataContainsFillableUsername(const PasswordFormFillData& fill_data) {
  return !fill_data.username_field.name.empty() &&
         (!fill_data.additional_logins.empty() ||
          !fill_data.username_field.value.empty());
}

// Checks if the prefilled value of the username element is one of the known
// values possibly used as placeholders. The list of possible placeholder
// values comes from popular sites exhibiting this issue.
// TODO(crbug.com/832622): Remove this once a stable solution is in place.
bool PossiblePrefilledUsernameValue(const std::string& username_value) {
  // Explicitly create a |StringFlatSet| when constructing
  // kPrefilledUsernameValues to work around GCC bug 84849, which causes the
  // initializer list not to be properly forwarded to base::flat_set's
  // constructor.
  using StringFlatSet = base::flat_set<std::string, std::less<>>;
  static base::NoDestructor<StringFlatSet> kPrefilledUsernameValues(
      StringFlatSet({"3~15个字符,中文字符7个以内",
                     "Benutzername",
                     "Digite seu CPF ou e-mail",
                     "DS Logon Username",
                     "Email Address",
                     "email address",
                     "Email masih kosong",
                     "Email/手機號碼",
                     "Enter User Name",
                     "Identifiant",
                     "Kullanıcı Adı",
                     "Kunden-ID",
                     "Nick",
                     "Nom Utilisateur",
                     "Rut",
                     "Siret",
                     "this is usually your email address",
                     "UID/用戶名/Email",
                     "User Id",
                     "User Name",
                     "Username",
                     "username",
                     "username or email",
                     "Username or email:",
                     "Username/Email",
                     "Usuario",
                     "Your email address",
                     "Имя",
                     "Логин",
                     "Логин...",
                     "כתובת דוא''ל",
                     "اسم العضو",
                     "اسم المستخدم",
                     "الاسم",
                     "نام کاربری",
                     "メールアドレス",
                     "用户名",
                     "用户名/Email",
                     "請輸入身份證字號",
                     "请用微博帐号登录",
                     "请输入手机号或邮箱",
                     "请输入邮箱或手机号",
                     "邮箱/手机/展位号"}));

  return kPrefilledUsernameValues->find(
             base::TrimWhitespaceASCII(username_value, base::TRIM_ALL)) !=
         kPrefilledUsernameValues->end();
}

// Returns true if password form has username and password fields with either
// same or no name and id attributes supplied.
bool DoesFormContainAmbiguousOrEmptyNames(
    const PasswordFormFillData& fill_data) {
  return (fill_data.username_field.name == fill_data.password_field.name) ||
         (fill_data.password_field.name ==
              base::ASCIIToUTF16(kDummyPasswordField) &&
          (!FillDataContainsFillableUsername(fill_data) ||
           fill_data.username_field.name ==
               base::ASCIIToUTF16(kDummyUsernameField)));
}

bool IsFieldPasswordField(const FormFieldData& field) {
  return (field.form_control_type == "password");
}

// Returns true if any password field within |control_elements| is supplied with
// either |autocomplete='current-password'| or |autocomplete='new-password'|
// attribute.
bool HasPasswordWithAutocompleteAttribute(
    const std::vector<WebFormControlElement>& control_elements) {
  for (const WebFormControlElement& control_element : control_elements) {
    if (!control_element.HasHTMLTagName("input"))
      continue;

    const WebInputElement input_element =
        control_element.ToConst<WebInputElement>();
    const AutocompleteFlag flag = AutocompleteFlagForElement(input_element);
    if (input_element.IsPasswordFieldForAutofill() &&
        (flag == AutocompleteFlag::CURRENT_PASSWORD ||
         flag == AutocompleteFlag::NEW_PASSWORD)) {
      return true;
    }
  }

  return false;
}

// Returns the |field|'s autofillable name. If |ambiguous_or_empty_names| is set
// to true returns a dummy name instead.
base::string16 FieldName(const FormFieldData& field,
                         bool ambiguous_or_empty_names) {
  return ambiguous_or_empty_names
             ? (IsFieldPasswordField(field)
                    ? base::ASCIIToUTF16(kDummyPasswordField)
                    : base::ASCIIToUTF16(kDummyUsernameField))
             : field.name;
}

bool IsUnownedPasswordFormVisible(const WebInputElement& input_element) {
  return !input_element.IsNull() &&
         form_util::IsWebElementVisible(input_element);
}

// Utility function to find the unique entry of |control_elements| for the
// specified input |field|. On successful find, adds it to |result| and returns
// |true|. Otherwise clears the references from each |HTMLInputElement| from
// |result| and returns |false|.
bool FindFormInputElement(
    const std::vector<WebFormControlElement>& control_elements,
    const FormFieldData& field,
    bool ambiguous_or_empty_names,
    FormInputElementMap* result) {
  // Match the first input element, if any.
  bool found_input = false;
  bool is_password_field = IsFieldPasswordField(field);
  bool does_password_field_has_ambigous_or_empty_name =
      ambiguous_or_empty_names && is_password_field;
  bool ambiguous_and_multiple_password_fields_with_autocomplete =
      does_password_field_has_ambigous_or_empty_name &&
      HasPasswordWithAutocompleteAttribute(control_elements);
  base::string16 field_name = FieldName(field, ambiguous_or_empty_names);
  for (const WebFormControlElement& control_element : control_elements) {
    if (!ambiguous_or_empty_names &&
        control_element.NameForAutofill().Utf16() != field_name) {
      continue;
    }

    if (!control_element.HasHTMLTagName("input"))
      continue;

    // Only fill saved passwords into password fields and usernames into text
    // fields.
    const WebInputElement input_element =
        control_element.ToConst<WebInputElement>();
    if (!input_element.IsTextField() ||
        input_element.IsPasswordFieldForAutofill() != is_password_field)
      continue;

    // For change password form with ambiguous or empty names keep only the
    // first password field having |autocomplete='current-password'| attribute
    // set. Also make sure we avoid keeping password fields having
    // |autocomplete='new-password'| attribute set.
    if (ambiguous_and_multiple_password_fields_with_autocomplete &&
        AutocompleteFlagForElement(input_element) !=
            AutocompleteFlag::CURRENT_PASSWORD) {
      continue;
    }

    // Check for a non-unique match.
    if (found_input) {
      // For change password form keep only the first password field entry.
      if (does_password_field_has_ambigous_or_empty_name) {
        if (!form_util::IsWebElementVisible((*result)[field_name])) {
          // If a previously chosen field was invisible then take the current
          // one.
          (*result)[field_name] = input_element;
        }
        continue;
      }

      found_input = false;
      break;
    }

    (*result)[field_name] = input_element;
    found_input = true;
  }

  // A required element was not found. This is not the right form.
  // Make sure no input elements from a partially matched form in this
  // iteration remain in the result set.
  // Note: clear will remove a reference from each InputElement.
  if (!found_input) {
    result->clear();
    return false;
  }

  return true;
}

// Helper to search through |control_elements| for the specified input elements
// in |data|, and add results to |result|.
bool FindFormInputElements(
    const std::vector<WebFormControlElement>& control_elements,
    const PasswordFormFillData& data,
    bool ambiguous_or_empty_names,
    FormInputElementMap* result) {
  return FindFormInputElement(control_elements, data.password_field,
                              ambiguous_or_empty_names, result) &&
         (!FillDataContainsFillableUsername(data) ||
          FindFormInputElement(control_elements, data.username_field,
                               ambiguous_or_empty_names, result));
}

// Helper to locate form elements identified by |data|.
void FindFormElements(content::RenderFrame* render_frame,
                      const PasswordFormFillData& data,
                      bool ambiguous_or_empty_names,
                      FormElementsList* results) {
  DCHECK(results);

  WebDocument doc = render_frame->GetWebFrame()->GetDocument();

  if (GetSignOnRealm(data.origin) !=
      GetSignOnRealm(form_util::GetCanonicalOriginForDocument(doc)))
    return;

  WebVector<WebFormElement> forms;
  doc.Forms(forms);

  for (size_t i = 0; i < forms.size(); ++i) {
    WebFormElement fe = forms[i];

    // Action URL must match.
    if (data.action != form_util::GetCanonicalActionForForm(fe))
      continue;

    std::vector<WebFormControlElement> control_elements =
        form_util::ExtractAutofillableElementsInForm(fe);
    FormInputElementMap cur_map;
    if (FindFormInputElements(control_elements, data, ambiguous_or_empty_names,
                              &cur_map))
      results->push_back(cur_map);
  }
  // If the element to be filled are not in a <form> element, the "action" and
  // origin should be the same.
  if (data.action != data.origin)
    return;

  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedAutofillableFormFieldElements(doc.All(), nullptr);
  FormInputElementMap unowned_elements_map;
  if (FindFormInputElements(control_elements, data, ambiguous_or_empty_names,
                            &unowned_elements_map))
    results->push_back(unowned_elements_map);
}

bool IsElementEditable(const WebInputElement& element) {
  return element.IsEnabled() && !element.IsReadOnly();
}

bool DoUsernamesMatch(const base::string16& potential_suggestion,
                      const base::string16& current_username,
                      bool exact_match) {
  if (potential_suggestion == current_username)
    return true;
  return !exact_match && IsPrefixOfEmailEndingWithAtSign(current_username,
                                                         potential_suggestion);
}

// Returns whether the given |element| is editable.
bool IsElementAutocompletable(const WebInputElement& element) {
  return IsElementEditable(element);
}

// Returns whether the |username_element| is allowed to be autofilled.
//
// Note that if the user interacts with the |password_field| and the
// |username_element| is user-defined (i.e., non-empty and non-autofilled), then
// this function returns false. This is a precaution, to not override the field
// if it has been classified as username by accident.
bool IsUsernameAmendable(const WebInputElement& username_element,
                         bool is_password_field_selected) {
  return !username_element.IsNull() &&
         IsElementAutocompletable(username_element) &&
         (!is_password_field_selected || username_element.IsAutofilled() ||
          username_element.Value().IsEmpty());
}

// Log a message including the name, method and action of |form|.
void LogHTMLForm(SavePasswordProgressLogger* logger,
                 SavePasswordProgressLogger::StringID message_id,
                 const WebFormElement& form) {
  logger->LogHTMLForm(message_id, form.GetName().Utf8(),
                      GURL(form.Action().Utf8()));
}


// Returns true if there are any suggestions to be derived from |fill_data|.
// Unless |show_all| is true, only considers suggestions with usernames having
// |current_username| as a prefix.
bool CanShowSuggestion(const PasswordFormFillData& fill_data,
                       const base::string16& current_username,
                       bool show_all) {
  base::string16 current_username_lower = base::i18n::ToLower(current_username);
  if (show_all ||
      base::StartsWith(base::i18n::ToLower(fill_data.username_field.value),
                       current_username_lower, base::CompareCase::SENSITIVE)) {
    return true;
  }

  for (const auto& login : fill_data.additional_logins) {
    if (show_all ||
        base::StartsWith(base::i18n::ToLower(login.first),
                         current_username_lower,
                         base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  return false;
}

// This function attempts to find the matching credentials for the
// |current_username| by scanning |fill_data|. The result is written in
// |username| and |password| parameters.
void FindMatchesByUsername(const PasswordFormFillData& fill_data,
                           const base::string16& current_username,
                           bool exact_username_match,
                           RendererSavePasswordProgressLogger* logger,
                           base::string16* username,
                           base::string16* password) {
  // Look for any suitable matches to current field text.
  if (DoUsernamesMatch(fill_data.username_field.value, current_username,
                       exact_username_match)) {
    *username = fill_data.username_field.value;
    *password = fill_data.password_field.value;
    if (logger)
      logger->LogMessage(Logger::STRING_USERNAMES_MATCH);
  } else {
    // Scan additional logins for a match.
    for (const auto& it : fill_data.additional_logins) {
      if (!it.second.realm.empty()) {
        // Non-empty realm means PSL match. Do not autofill PSL matched
        // credentials. The reason for this is that PSL matched sites are
        // different sites, so a password for a PSL matched site should be never
        // filled without explicit user selection.
        continue;
      }
      if (DoUsernamesMatch(it.first, current_username, exact_username_match)) {
        *username = it.first;
        *password = it.second.password;
        break;
      }
    }
    if (logger) {
      logger->LogBoolean(Logger::STRING_MATCH_IN_ADDITIONAL,
                         !(username->empty() && password->empty()));
    }
  }
}

// TODO(crbug.com/564578): This duplicates code from
// components/password_manager/core/browser/psl_matching_helper.h. The logic
// using this code should ultimately end up in
// components/password_manager/core/browser, at which point it can use the
// original code directly.
std::string GetRegistryControlledDomain(const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// TODO(crbug.com/564578): This duplicates code from
// components/password_manager/core/browser/psl_matching_helper.h. The logic
// using this code should ultimately end up in
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

// Helper function that calculates form signature for |password_form| and
// returns it as WebString.
WebString GetFormSignatureAsWebString(const PasswordForm& password_form) {
  return WebString::FromUTF8(
      base::NumberToString(CalculateFormSignature(password_form.form_data)));
}

// Add parser annotations saved in |password_form| to |element|.
void AddParserAnnotations(PasswordForm* password_form,
                          blink::WebFormControlElement* element) {
  base::string16 element_name = element->NameForAutofill().Utf16();
  std::string attribute_value;
  if (password_form->username_element == element_name) {
    attribute_value = "username_element";
  } else if (password_form->password_element == element_name) {
    attribute_value = "password_element";
  } else if (password_form->new_password_element == element_name) {
    attribute_value = "new_password_element";
  } else if (password_form->confirmation_password_element == element_name) {
    attribute_value = "confirmation_password_element";
  }
  element->SetAttribute(
      blink::WebString::FromASCII(kDebugAttributeForParserAnnotations),
      attribute_value.empty() ? blink::WebString()
                              : blink::WebString::FromASCII(attribute_value));
}

// Annotate |fields| with field signatures and form signature as HTML
// attributes.
void AnnotateFieldsWithSignatures(
    std::vector<blink::WebFormControlElement>* fields,
    const blink::WebString& form_signature,
    PasswordForm* password_form) {
  for (blink::WebFormControlElement& control_element : *fields) {
    FieldSignature field_signature = CalculateFieldSignatureByNameAndType(
        control_element.NameForAutofill().Utf16(),
        control_element.FormControlTypeForAutofill().Utf8());
    control_element.SetAttribute(
        WebString::FromASCII(kDebugAttributeForFieldSignature),
        WebString::FromUTF8(base::NumberToString(field_signature)));
    control_element.SetAttribute(
        blink::WebString::FromASCII(kDebugAttributeForFormSignature),
        form_signature);
    if (password_form)
      AddParserAnnotations(password_form, &control_element);
  }
}

// Annotate |forms| and all fields in the |frame| with form and field signatures
// as HTML attributes.
void AnnotateFormsAndFieldsWithSignatures(WebLocalFrame* frame,
                                          WebVector<WebFormElement>* forms) {
  for (WebFormElement& form : *forms) {
    std::unique_ptr<PasswordForm> password_form(
        CreatePasswordFormFromWebForm(form, nullptr, nullptr, nullptr));
    WebString form_signature;
    if (password_form) {
      form_signature = GetFormSignatureAsWebString(*password_form);
      form.SetAttribute(WebString::FromASCII(kDebugAttributeForFormSignature),
                        form_signature);
    }
    std::vector<WebFormControlElement> form_fields =
        form_util::ExtractAutofillableElementsInForm(form);
    AnnotateFieldsWithSignatures(&form_fields, form_signature,
                                 password_form ? password_form.get() : nullptr);
  }

  std::vector<WebFormControlElement> unowned_elements =
      form_util::GetUnownedAutofillableFormFieldElements(
          frame->GetDocument().All(), nullptr);
  std::unique_ptr<PasswordForm> password_form(
      CreatePasswordFormFromUnownedInputElements(*frame, nullptr, nullptr,
                                                 nullptr));
  WebString form_signature;
  if (password_form)
    form_signature = GetFormSignatureAsWebString(*password_form);
  AnnotateFieldsWithSignatures(&unowned_elements, form_signature,
                               password_form ? password_form.get() : nullptr);
}

// Returns true iff there is a password field in |frame|.
bool HasPasswordField(const WebLocalFrame& frame) {
  static base::NoDestructor<WebString> kPassword("password");

  const WebElementCollection elements = frame.GetDocument().All();
  for (WebElement element = elements.FirstItem(); !element.IsNull();
       element = elements.NextItem()) {
    if (element.IsFormControlElement()) {
      const WebFormControlElement& control =
          element.To<WebFormControlElement>();
      if (control.FormControlTypeForAutofill() == *kPassword)
        return true;
    }
  }
  return false;
}

// Returns the closest visible autocompletable non-password text element
// preceding the |password_element| either in a form, if it belongs to one, or
// in the |frame|.
WebInputElement FindUsernameElementPrecedingPasswordElement(
    WebLocalFrame* frame,
    const WebInputElement& password_element) {
  DCHECK(!password_element.IsNull());

  std::vector<WebFormControlElement> elements;
  if (password_element.Form().IsNull()) {
    elements = form_util::GetUnownedAutofillableFormFieldElements(
        frame->GetDocument().All(), nullptr);
  } else {
    WebVector<WebFormControlElement> web_control_elements;
    password_element.Form().GetFormControlElements(web_control_elements);
    elements.assign(web_control_elements.begin(), web_control_elements.end());
  }

  auto iter = std::find(elements.begin(), elements.end(), password_element);
  if (iter == elements.end())
    return WebInputElement();

  for (auto begin = elements.begin(); iter != begin;) {
    --iter;
    const WebInputElement* input = ToWebInputElement(&*iter);
    if (input && input->IsTextField() && !input->IsPasswordFieldForAutofill() &&
        IsElementAutocompletable(*input) &&
        form_util::IsWebElementVisible(*input)) {
      return *input;
    }
  }

  return WebInputElement();
}

WebInputElement ConvertToWebInput(const WebFormControlElement& element) {
  if (element.IsNull())
    return WebInputElement();
  const WebInputElement* input = ToWebInputElement(&element);
  return input ? *input : WebInputElement();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, public:

PasswordAutofillAgent::PasswordAutofillAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      last_supplied_password_info_iter_(web_input_to_password_info_.end()),
      logging_state_active_(false),
      username_autofill_state_(WebAutofillState::kNotFilled),
      password_autofill_state_(WebAutofillState::kNotFilled),
      sent_request_to_store_(false),
      checked_safe_browsing_reputation_(false),
      focus_state_notifier_(this),
      binding_(this) {
  registry->AddInterface(
      base::Bind(&PasswordAutofillAgent::BindRequest, base::Unretained(this)));
}

PasswordAutofillAgent::~PasswordAutofillAgent() {
  AutofillAgent* agent = autofill_agent_.get();
  if (agent)
    agent->RemoveFormObserver(this);
}

void PasswordAutofillAgent::BindRequest(
    mojom::PasswordAutofillAgentAssociatedRequest request) {
  binding_.Bind(std::move(request));
}

void PasswordAutofillAgent::SetAutofillAgent(AutofillAgent* autofill_agent) {
  AutofillAgent* agent = autofill_agent_.get();
  if (agent)
    agent->RemoveFormObserver(this);
  autofill_agent_ = autofill_agent->GetWeakPtr();
  autofill_agent->AddFormObserver(this);
}

void PasswordAutofillAgent::SetPasswordGenerationAgent(
    PasswordGenerationAgent* generation_agent) {
  password_generation_agent_ = generation_agent;
}

PasswordAutofillAgent::FocusStateNotifier::FocusStateNotifier(
    PasswordAutofillAgent* agent)
    : was_fillable_(false), was_password_field_(false), agent_(agent) {}

PasswordAutofillAgent::FocusStateNotifier::~FocusStateNotifier() = default;

void PasswordAutofillAgent::FocusStateNotifier::FocusedInputChanged(
    bool is_fillable,
    bool is_password_field) {
  // Forward the request, if the field is valid or the request is different.
  if (!is_fillable && !was_fillable_ && !is_password_field &&
      !was_password_field_) {
    return;  // A previous request already reported this exact state.
  }
  was_fillable_ = is_fillable;
  was_password_field_ = is_password_field;
  agent_->GetPasswordManagerDriver()->FocusedInputChanged(is_fillable,
                                                          is_password_field);
}

PasswordAutofillAgent::PasswordValueGatekeeper::PasswordValueGatekeeper()
    : was_user_gesture_seen_(false) {
}

PasswordAutofillAgent::PasswordValueGatekeeper::~PasswordValueGatekeeper() {
}

void PasswordAutofillAgent::PasswordValueGatekeeper::RegisterElement(
    WebInputElement* element) {
  if (was_user_gesture_seen_)
    ShowValue(element);
  else
    elements_.push_back(*element);
}

void PasswordAutofillAgent::PasswordValueGatekeeper::OnUserGesture() {
  if (was_user_gesture_seen_)
    return;

  was_user_gesture_seen_ = true;

  for (WebInputElement& element : elements_)
    ShowValue(&element);

  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::Reset() {
  was_user_gesture_seen_ = false;
  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::ShowValue(
    WebInputElement* element) {
  if (!element->IsNull() && !element->SuggestedValue().IsEmpty()) {
    element->SetAutofillValue(element->SuggestedValue());
    element->SetAutofillState(WebAutofillState::kAutofilled);
  }
}

bool PasswordAutofillAgent::TextDidChangeInTextField(
    const WebInputElement& element) {
  // TODO(vabr): Get a mutable argument instead. http://crbug.com/397083
  WebInputElement mutable_element = element;  // We need a non-const.
  mutable_element.SetAutofillState(WebAutofillState::kNotFilled);

  auto iter = web_input_to_password_info_.find(element);
  if (iter != web_input_to_password_info_.end()) {
    iter->second.password_was_edited_last = false;
  }

  // Show the popup with the list of available usernames.
  return ShowSuggestions(element, false, false);
}

void PasswordAutofillAgent::DidEndTextFieldEditing() {
  focus_state_notifier_.FocusedInputChanged(false, false);
}

void PasswordAutofillAgent::UpdateStateForTextChange(
    const WebInputElement& element) {
  // TODO(vabr): Get a mutable argument instead. http://crbug.com/397083
  WebInputElement mutable_element = element;  // We need a non-const.

  if (element.IsTextField()) {
    const base::string16 element_value = element.Value().Utf16();
    field_data_manager_.UpdateFieldDataMap(element, element_value,
                                           FieldPropertiesFlags::USER_TYPED);
  }

  ProvisionallySavePassword(element.Form(), element, RESTRICTION_NONE);

  if (element.IsPasswordFieldForAutofill()) {
    auto iter = password_to_username_.find(element);
    if (iter != password_to_username_.end()) {
      web_input_to_password_info_[iter->second].password_was_edited_last = true;
      // Note that the suggested value of |mutable_element| was reset when its
      // value changed.
      mutable_element.SetAutofillState(WebAutofillState::kNotFilled);
    }
  }

  if (element.IsPasswordFieldForAutofill())
    GetPasswordManagerDriver()->UserModifiedPasswordField();
}

bool PasswordAutofillAgent::FillSuggestion(
    const WebFormControlElement& control_element,
    const base::string16& username,
    const base::string16& password) {
  // The element in context of the suggestion popup.
  const WebInputElement* element = ToWebInputElement(&control_element);
  if (!element)
    return false;

  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info = nullptr;

  if (!FindPasswordInfoForElement(*element, &username_element,
                                  &password_element, &password_info) ||
      !IsElementAutocompletable(password_element)) {
    return false;
  }

  password_info->password_was_edited_last = false;
  if (element->IsPasswordFieldForAutofill()) {
    password_info->password_field_suggestion_was_accepted = true;
    password_info->password_field = password_element;
  }

  // Call OnFieldAutofilled before WebInputElement::SetAutofillState which may
  // cause frame closing.
  if (password_generation_agent_)
    password_generation_agent_->OnFieldAutofilled(password_element);

  if (IsUsernameAmendable(username_element,
                          element->IsPasswordFieldForAutofill()) &&
      username_element.Value().Utf16() != username) {
    FillField(&username_element, username);
  }

  FillPasswordFieldAndSave(&password_element, password);

  WebInputElement mutable_filled_element = *element;
  mutable_filled_element.SetSelectionRange(element->Value().length(),
                                           element->Value().length());

  return true;
}

void PasswordAutofillAgent::FillIntoFocusedField(
    bool is_password,
    const base::string16& credential,
    FillIntoFocusedFieldCallback callback) {
  if (focused_input_element_.IsNull()) {
    std::move(callback).Run(autofill::FillingStatus::ERROR_NO_VALID_FIELD);
    return;
  }
  if (is_password) {
    if (!focused_input_element_.IsPasswordFieldForAutofill()) {
      std::move(callback).Run(autofill::FillingStatus::ERROR_NOT_ALLOWED);
      return;
    }
    FillPasswordFieldAndSave(&focused_input_element_, credential);
  } else {
    FillField(&focused_input_element_, credential);
  }
  std::move(callback).Run(autofill::FillingStatus::SUCCESS);
}

void PasswordAutofillAgent::FillField(WebInputElement* input,
                                      const base::string16& credential) {
  DCHECK(input);
  DCHECK(!input->IsNull());
  input->SetAutofillValue(WebString::FromUTF16(credential));
  input->SetAutofillState(WebAutofillState::kAutofilled);
  field_data_manager_.UpdateFieldDataMap(
      *input, credential, FieldPropertiesFlags::AUTOFILLED_ON_USER_TRIGGER);
}

void PasswordAutofillAgent::FillPasswordFieldAndSave(
    WebInputElement* password_input,
    const base::string16& credential) {
  DCHECK(password_input);
  DCHECK(password_input->IsPasswordFieldForAutofill());
  FillField(password_input, credential);
  ProvisionallySavePassword(password_input->Form(), *password_input,
                            RESTRICTION_NONE);
}

bool PasswordAutofillAgent::PreviewSuggestion(
    const WebFormControlElement& control_element,
    const WebString& username,
    const WebString& password) {
  // The element in context of the suggestion popup.
  const WebInputElement* element = ToWebInputElement(&control_element);
  if (!element)
    return false;

  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info;

  if (!FindPasswordInfoForElement(*element, &username_element,
                                  &password_element, &password_info) ||
      !IsElementAutocompletable(password_element)) {
    return false;
  }

  if (IsUsernameAmendable(username_element,
                          element->IsPasswordFieldForAutofill())) {
    if (username_query_prefix_.empty())
      username_query_prefix_ = username_element.Value().Utf16();

    username_autofill_state_ = username_element.GetAutofillState();
    username_element.SetSuggestedValue(username);
    username_element.SetAutofillState(WebAutofillState::kPreviewed);
    form_util::PreviewSuggestion(username_element.SuggestedValue().Utf16(),
                                 username_query_prefix_, &username_element);
  }
  password_autofill_state_ = password_element.GetAutofillState();
  password_element.SetSuggestedValue(password);
  password_element.SetAutofillState(WebAutofillState::kPreviewed);

  return true;
}

bool PasswordAutofillAgent::DidClearAutofillSelection(
    const WebFormControlElement& control_element) {
  const WebInputElement* element = ToWebInputElement(&control_element);
  if (!element)
    return false;

  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info;

  if (!FindPasswordInfoForElement(*element, &username_element,
                                  &password_element, &password_info)) {
    return false;
  }

  ClearPreview(&username_element, &password_element);
  return true;
}

bool PasswordAutofillAgent::FindPasswordInfoForElement(
    const WebInputElement& element,
    WebInputElement* username_element,
    WebInputElement* password_element,
    PasswordInfo** password_info) {
  DCHECK(username_element && password_element && password_info);
  username_element->Reset();
  password_element->Reset();
  if (!element.IsPasswordFieldForAutofill()) {
    *username_element = element;
  } else {
    // If there is a password field, but a request to the store hasn't been sent
    // yet, then do fetch saved credentials now.
    if (!sent_request_to_store_) {
      SendPasswordForms(false);
      return false;
    }

    *password_element = element;

    auto iter = web_input_to_password_info_.find(element);
    if (iter == web_input_to_password_info_.end()) {
      PasswordToLoginMap::const_iterator password_iter =
          password_to_username_.find(element);
      if (password_iter == password_to_username_.end()) {
        if (web_input_to_password_info_.empty())
          return false;
        iter = last_supplied_password_info_iter_;
      } else {
        *username_element = password_iter->second;
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
    // Otherwise |username_element| has been set above.
  }

  auto iter = web_input_to_password_info_.find(*username_element);
  if (iter == web_input_to_password_info_.end())
    return false;

  *password_info = &iter->second;
  if (password_element->IsNull())
    *password_element = (*password_info)->password_field;

  return true;
}

bool PasswordAutofillAgent::IsUsernameOrPasswordField(
    const WebInputElement& element) {
  // Note: A site may use a Password field to collect a CVV or a Credit Card
  // number, but showing a slightly misleading warning here is better than
  // showing no warning at all.
  if (element.IsPasswordFieldForAutofill())
    return true;

  // If a field declares itself a username input, show the warning.
  if (AutocompleteFlagForElement(element) == AutocompleteFlag::USERNAME)
    return true;

  // Otherwise, analyze the form and return true if this input element seems
  // to be the username field.
  std::unique_ptr<PasswordForm> password_form;
  if (element.Form().IsNull()) {
    // To double check that element's frame and |render_frame()->GetWebFrame()|
    // which is used in |GetPasswordFormFromUnownedInputElements| are identical.
    DCHECK_EQ(element.GetDocument().GetFrame(), render_frame()->GetWebFrame());
    password_form = GetPasswordFormFromUnownedInputElements();
  } else {
    password_form = GetPasswordFormFromWebForm(element.Form());
  }

  if (!password_form)
    return false;
  return (password_form->username_element == element.NameForAutofill().Utf16());
}

bool PasswordAutofillAgent::ShowSuggestions(const WebInputElement& element,
                                            bool show_all,
                                            bool generation_popup_showing) {
  WebInputElement username_element;
  WebInputElement password_element;
  PasswordInfo* password_info;

  if (!FindPasswordInfoForElement(element, &username_element, &password_element,
                                  &password_info)) {
    if (IsUsernameOrPasswordField(element)) {
      WebLocalFrame* frame = render_frame()->GetWebFrame();
      GURL frame_url = GURL(frame->GetDocument().Url());
#if defined(SAFE_BROWSING_DB_LOCAL)
      if (!checked_safe_browsing_reputation_) {
        checked_safe_browsing_reputation_ = true;
        GURL action_url =
            element.Form().IsNull()
                ? GURL()
                : form_util::GetCanonicalActionForForm(element.Form());
        GetPasswordManagerDriver()->CheckSafeBrowsingReputation(action_url,
                                                                frame_url);
      }
#endif
    }
    return false;
  }

  // If autocomplete='off' is set on the form elements, no suggestion dialog
  // should be shown. However, return |true| to indicate that this is a known
  // password form and that the request to show suggestions has been handled (as
  // a no-op).
  if (!element.IsTextField() || !IsElementAutocompletable(element) ||
      !IsElementAutocompletable(password_element))
    return true;

  if (element.NameForAutofill().IsEmpty() &&
      !DoesFormContainAmbiguousOrEmptyNames(password_info->fill_data)) {
    return false;  // If the field has no name, then we won't have values.
  }

  // Don't attempt to autofill with values that are too large.
  if (element.Value().length() > kMaximumTextSizeForAutocomplete)
    return false;

  // If the element is a password field, do not to show a popup if the user has
  // already accepted a password suggestion on another password field.
  if (element.IsPasswordFieldForAutofill() &&
      (password_info->password_field_suggestion_was_accepted &&
       element != password_info->password_field))
    return true;

  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.AutocompletePopupSuppressedByGeneration",
      generation_popup_showing);

  if (generation_popup_showing)
    return false;

  // Chrome should never show more than one account for a password element since
  // this implies that the username element cannot be modified. Thus even if
  // |show_all| is true, check if the element in question is a password element
  // for the call to ShowSuggestionPopup.
  return ShowSuggestionPopup(*password_info, element,
                             show_all && !element.IsPasswordFieldForAutofill(),
                             element.IsPasswordFieldForAutofill());
}

bool PasswordAutofillAgent::FrameCanAccessPasswordManager() {
  // about:blank or about:srcdoc frames should not be allowed to use password
  // manager.  See https://crbug.com/756587.
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  blink::WebURL url = frame->GetDocument().Url();
  if (!url.ProtocolIs(url::kHttpScheme) && !url.ProtocolIs(url::kHttpsScheme))
    return false;
  return frame->GetSecurityOrigin().CanAccessPasswordManager();
}

void PasswordAutofillAgent::OnDynamicFormsSeen() {
  SendPasswordForms(false /* only_visible */);
}

void PasswordAutofillAgent::FireSubmissionIfFormDisappear(
    PasswordForm::SubmissionIndicatorEvent event) {
  if (!provisionally_saved_form_.IsPasswordValid())
    return;

  DCHECK(FrameCanAccessPasswordManager());

  // Prompt to save only if the form is now gone, either invisible or
  // removed from the DOM.
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  const auto& password_form = provisionally_saved_form_.password_form();
  // TODO(crbug.com/720347): This method could be called often and checking form
  // visibility could be expesive. Add performance metrics for this.
  if (event != PasswordForm::SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR) {
    if (form_util::IsFormVisible(frame,
                                 provisionally_saved_form_.form_element(),
                                 password_form.action, password_form.origin,
                                 password_form.form_data) ||
        (provisionally_saved_form_.form_element().IsNull() &&
         IsUnownedPasswordFormVisible(
             provisionally_saved_form_.input_element()))) {
      return;
    }
  }

  provisionally_saved_form_.SetSubmissionIndicatorEvent(event);
  GetPasswordManagerDriver()->SameDocumentNavigation(password_form);
  provisionally_saved_form_.Reset();
}

void PasswordAutofillAgent::UserGestureObserved() {
  gatekeeper_.OnUserGesture();
}

void PasswordAutofillAgent::SendPasswordForms(bool only_visible) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_SEND_PASSWORD_FORMS_METHOD);
    logger->LogBoolean(Logger::STRING_ONLY_VISIBLE, only_visible);
  }

  WebLocalFrame* frame = render_frame()->GetWebFrame();

  // Make sure that this security origin is allowed to use password manager.
  blink::WebSecurityOrigin origin = frame->GetDocument().GetSecurityOrigin();
  if (logger) {
    logger->LogURL(Logger::STRING_SECURITY_ORIGIN,
                   GURL(origin.ToString().Utf8()));
  }
  if (!FrameCanAccessPasswordManager()) {
    if (logger)
      logger->LogMessage(Logger::STRING_SECURITY_ORIGIN_FAILURE);
    return;
  }

  // Checks whether the webpage is a redirect page or an empty page.
  if (form_util::IsWebpageEmpty(frame)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_WEBPAGE_EMPTY);
    }
    return;
  }

  WebVector<WebFormElement> forms;
  frame->GetDocument().Forms(forms);

  if (IsShowAutofillSignaturesEnabled())
    AnnotateFormsAndFieldsWithSignatures(frame, &forms);
  if (logger)
    logger->LogNumber(Logger::STRING_NUMBER_OF_ALL_FORMS, forms.size());

  std::vector<PasswordForm> password_forms;
  for (const WebFormElement& form : forms) {
    if (only_visible) {
      bool is_form_visible = form_util::AreFormContentsVisible(form);
      if (logger) {
        LogHTMLForm(logger.get(), Logger::STRING_FORM_FOUND_ON_PAGE, form);
        logger->LogBoolean(Logger::STRING_FORM_IS_VISIBLE, is_form_visible);
      }

      // If requested, ignore non-rendered forms, e.g., those styled with
      // display:none.
      if (!is_form_visible)
        continue;
    }

    std::unique_ptr<PasswordForm> password_form(
        GetPasswordFormFromWebForm(form));
    if (password_form) {
      if (logger) {
        logger->LogPasswordForm(Logger::STRING_FORM_IS_PASSWORD,
                                *password_form);
      }
      password_forms.push_back(*password_form);
    }
  }

  // See if there are any unattached input elements that could be used for
  // password submission.
  bool add_unowned_inputs = true;
  if (only_visible) {
    std::vector<WebFormControlElement> control_elements =
        form_util::GetUnownedAutofillableFormFieldElements(
            frame->GetDocument().All(), nullptr);
    add_unowned_inputs =
        form_util::IsSomeControlElementVisible(control_elements);
    if (logger) {
      logger->LogBoolean(Logger::STRING_UNOWNED_INPUTS_VISIBLE,
                         add_unowned_inputs);
    }
  }
  if (add_unowned_inputs) {
    std::unique_ptr<PasswordForm> password_form(
        GetPasswordFormFromUnownedInputElements());
    if (password_form) {
      if (logger) {
        logger->LogPasswordForm(Logger::STRING_FORM_IS_PASSWORD,
                                *password_form);
      }
      password_forms.push_back(*password_form);
    }
  }

  if (only_visible) {
    // Send the PasswordFormsRendered message regardless of whether
    // |password_forms| is empty. The empty |password_forms| are a possible
    // signal to the browser that a pending login attempt succeeded.
    WebFrame* main_frame = render_frame()->GetWebFrame()->Top();
    bool did_stop_loading = !main_frame || !main_frame->IsLoading();
    GetPasswordManagerDriver()->PasswordFormsRendered(password_forms,
                                                      did_stop_loading);
  } else {
    // If there is a password field, but the list of password forms is empty for
    // some reason, add a dummy form to the list. It will cause a request to the
    // store. Therefore, saved passwords will be available for filling on click.
    if (!sent_request_to_store_ && password_forms.empty() &&
        HasPasswordField(*frame)) {
      // Set everything that |FormDigest| needs.
      password_forms.push_back(PasswordForm());
      password_forms.back().scheme = PasswordForm::SCHEME_HTML;
      password_forms.back().origin =
          form_util::GetCanonicalOriginForDocument(frame->GetDocument());
      password_forms.back().signon_realm =
          GetSignOnRealm(password_forms.back().origin);
    }
    if (!password_forms.empty()) {
      sent_request_to_store_ = true;
      GetPasswordManagerDriver()->PasswordFormsParsed(password_forms);
    }
  }

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Provide warnings about the accessibility of password forms on the page.
  if (!password_forms.empty() &&
      (frame->GetDocument().Url().ProtocolIs(url::kHttpScheme) ||
       frame->GetDocument().Url().ProtocolIs(url::kHttpsScheme)))
    page_passwords_analyser_.AnalyseDocumentDOM(frame);
#endif
}

void PasswordAutofillAgent::DidFinishDocumentLoad() {
  SendPasswordForms(false);
}

void PasswordAutofillAgent::DidFinishLoad() {
  // The |frame| contents have been rendered.  Let the PasswordManager know
  // which of the loaded frames are actually visible to the user.  This also
  // triggers the "Save password?" infobar if the user just submitted a password
  // form.
  SendPasswordForms(true);
}

void PasswordAutofillAgent::WillCommitProvisionalLoad() {
  FrameClosing();
}

void PasswordAutofillAgent::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  if (!is_same_document_navigation) {
    checked_safe_browsing_reputation_ = false;
  }
}

void PasswordAutofillAgent::OnFrameDetached() {
  // If a sub frame has been destroyed while the user was entering information
  // into a password form, try to save the data. See https://crbug.com/450806
  // for examples of sites that perform login using this technique.
  if (render_frame()->GetWebFrame()->Parent() &&
      provisionally_saved_form_.IsPasswordValid()) {
    DCHECK(FrameCanAccessPasswordManager());
    provisionally_saved_form_.SetSubmissionIndicatorEvent(
        PasswordForm::SubmissionIndicatorEvent::FRAME_DETACHED);
    GetPasswordManagerDriver()->SameDocumentNavigation(
        provisionally_saved_form_.password_form());
  }
  FrameClosing();
}

void PasswordAutofillAgent::OnWillSubmitForm(const WebFormElement& form) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_WILL_SUBMIT_FORM_METHOD);
    LogHTMLForm(logger.get(), Logger::STRING_HTML_FORM_FOR_SUBMIT, form);
  }

  std::unique_ptr<PasswordForm> submitted_form =
      GetPasswordFormFromWebForm(form);

  // If there is a provisionally saved password, copy over the previous
  // password value so we get the user's typed password, not the value that
  // may have been transformed for submit.
  // TODO(gcasto): Do we need to have this action equality check? Is it trying
  // to prevent accidentally copying over passwords from a different form?
  if (submitted_form) {
    if (logger) {
      logger->LogPasswordForm(Logger::STRING_CREATED_PASSWORD_FORM,
                              *submitted_form);
    }
    if (provisionally_saved_form_.IsSet() &&
        submitted_form->action ==
            provisionally_saved_form_.password_form().action) {
      if (logger)
        logger->LogMessage(Logger::STRING_SUBMITTED_PASSWORD_REPLACED);
      const auto& saved_form = provisionally_saved_form_.password_form();
      submitted_form->password_value = saved_form.password_value;
      submitted_form->new_password_value = saved_form.new_password_value;
      submitted_form->username_value = saved_form.username_value;
      submitted_form->submission_event =
          PasswordForm::SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
    }

    if (FrameCanAccessPasswordManager()) {
      // Some observers depend on sending this information now instead of when
      // the frame starts loading. If there are redirects that cause a new
      // RenderView to be instantiated (such as redirects to the WebStore)
      // we will never get to finish the load.
      GetPasswordManagerDriver()->PasswordFormSubmitted(*submitted_form);
    } else {
      if (logger)
        logger->LogMessage(Logger::STRING_SECURITY_ORIGIN_FAILURE);

      // Record how often users submit passwords on about:blank frames.
      if (form.GetDocument().Url().ProtocolIs(url::kAboutScheme)) {
        bool is_main_frame = !form.GetDocument().GetFrame()->Parent();
        UMA_HISTOGRAM_BOOLEAN("PasswordManager.AboutBlankPasswordSubmission",
                              is_main_frame);
      }
    }

    provisionally_saved_form_.Reset();
  } else if (logger) {
    logger->LogMessage(Logger::STRING_FORM_IS_NOT_PASSWORD);
  }
}

void PasswordAutofillAgent::OnDestruct() {
  binding_.Close();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void PasswordAutofillAgent::DidStartProvisionalLoad(
    blink::WebDocumentLoader* document_loader,
    bool is_content_initiated) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_DID_START_PROVISIONAL_LOAD_METHOD);
  }

  WebLocalFrame* navigated_frame = render_frame()->GetWebFrame();
  if (navigated_frame->Parent()) {
    if (logger)
      logger->LogMessage(Logger::STRING_FRAME_NOT_MAIN_FRAME);
    return;
  }

  // This is a new navigation, so require a new user gesture before filling in
  // passwords.
  gatekeeper_.Reset();
}

void PasswordAutofillAgent::OnProbablyFormSubmitted() {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_DID_START_PROVISIONAL_LOAD_METHOD);
  }

  if (!FrameCanAccessPasswordManager()) {
    if (logger)
      logger->LogMessage(Logger::STRING_SECURITY_ORIGIN_FAILURE);
    return;
  }

  // If onsubmit has been called, try and save that form.
  if (provisionally_saved_form_.IsSet()) {
    if (logger) {
      logger->LogPasswordForm(Logger::STRING_PROVISIONALLY_SAVED_FORM_FOR_FRAME,
                              provisionally_saved_form_.password_form());
    }
    provisionally_saved_form_.SetSubmissionIndicatorEvent(
        PasswordForm::SubmissionIndicatorEvent::
            PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD);
    GetPasswordManagerDriver()->PasswordFormSubmitted(
        provisionally_saved_form_.password_form());
    provisionally_saved_form_.Reset();
  }
}

void PasswordAutofillAgent::FillUsingRendererIDs(
    const PasswordFormFillData& form_data) {
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_ON_FILL_PASSWORD_FORM_METHOD);
  }
  WebInputElement username_element, password_element;
  std::tie(username_element, password_element) =
      FindUsernamePasswordElements(form_data);
  if (password_element.IsNull()) {
    MaybeStoreFallbackData(form_data);
    return;
  }

  StoreDataForFillOnAccountSelect(form_data, username_element,
                                  password_element);
  FillFormOnPasswordReceived(form_data, username_element, password_element,
                             &field_data_manager_, logger.get());
}

// mojom::PasswordAutofillAgent:
void PasswordAutofillAgent::FillPasswordForm(
    const PasswordFormFillData& form_data) {
  if (form_data.has_renderer_ids) {
    FillUsingRendererIDs(form_data);
    return;
  }

  std::vector<WebInputElement> elements;
  std::unique_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(
        GetPasswordManagerDriver().get()));
    logger->LogMessage(Logger::STRING_ON_FILL_PASSWORD_FORM_METHOD);
  }
  GetFillableElementFromFormData(form_data, logger.get(), &elements);

  // If wait_for_username is true, we don't want to initially fill the form
  // until the user types in a valid username.
  if (form_data.wait_for_username)
    return;

  for (auto element : elements) {
    WebInputElement username_element = !element.IsPasswordFieldForAutofill()
                                           ? element
                                           : password_to_username_[element];
    WebInputElement password_element =
        element.IsPasswordFieldForAutofill()
            ? element
            : web_input_to_password_info_[element].password_field;
    FillFormOnPasswordReceived(form_data, username_element, password_element,
                               &field_data_manager_, logger.get());
  }
}

void PasswordAutofillAgent::GetFillableElementFromFormData(
    const PasswordFormFillData& form_data,
    RendererSavePasswordProgressLogger* logger,
    std::vector<WebInputElement>* elements) {
  DCHECK(elements);
  bool ambiguous_or_empty_names =
      DoesFormContainAmbiguousOrEmptyNames(form_data);
  FormElementsList forms;
  FindFormElements(render_frame(), form_data, ambiguous_or_empty_names, &forms);
  if (logger) {
    logger->LogBoolean(Logger::STRING_AMBIGUOUS_OR_EMPTY_NAMES,
                       ambiguous_or_empty_names);
    logger->LogNumber(Logger::STRING_NUMBER_OF_POTENTIAL_FORMS_TO_FILL,
                      forms.size());
    logger->LogBoolean(Logger::STRING_FORM_DATA_WAIT,
                       form_data.wait_for_username);
  }
  for (const auto& form : forms) {
    base::string16 username_field_name;
    base::string16 password_field_name =
        FieldName(form_data.password_field, ambiguous_or_empty_names);
    bool form_contains_fillable_username_field =
        FillDataContainsFillableUsername(form_data);
    if (form_contains_fillable_username_field) {
      username_field_name =
          FieldName(form_data.username_field, ambiguous_or_empty_names);
    }
    if (logger) {
      logger->LogBoolean(Logger::STRING_CONTAINS_FILLABLE_USERNAME_FIELD,
                         form_contains_fillable_username_field);
      logger->LogBoolean(Logger::STRING_USERNAME_FIELD_NAME_EMPTY,
                         username_field_name.empty());
      logger->LogBoolean(Logger::STRING_PASSWORD_FIELD_NAME_EMPTY,
                         password_field_name.empty());
    }

    // Attach autocomplete listener to enable selecting alternate logins.
    WebInputElement username_element;
    WebInputElement password_element;

    // Check whether the password form has a username input field.
    if (!username_field_name.empty()) {
      const auto it = form.find(username_field_name);
      DCHECK(it != form.end());
      username_element = it->second;
    }

    // No password field, bail out.
    if (password_field_name.empty())
      break;

    // Get pointer to password element. (We currently only support single
    // password forms).
    {
      const auto it = form.find(password_field_name);
      DCHECK(it != form.end());
      password_element = it->second;
    }

    WebInputElement main_element =
        username_element.IsNull() ? password_element : username_element;
    if (elements)
      elements->push_back(main_element);
    StoreDataForFillOnAccountSelect(form_data, username_element,
                                    password_element);
  }

  MaybeStoreFallbackData(form_data);
}

void PasswordAutofillAgent::FocusedNodeHasChanged(const blink::WebNode& node) {
  focused_input_element_.Reset();
  if (node.IsNull() || !node.IsElementNode()) {  // Not a valid WebElement.
    focus_state_notifier_.FocusedInputChanged(
        /*is_fillable=*/false, /*is_password_field=*/false);
    return;
  }

  WebElement web_element = node.ToConst<WebElement>();
  const WebInputElement* input = ToWebInputElement(&web_element);
  if (!input) {
    focus_state_notifier_.FocusedInputChanged(
        /*is_fillable=*/false, /*is_password_field=*/false);
    return;  // If the node isn't an element, don't even try to convert.
  }
  bool is_password = false;
  bool is_fillable = input->IsTextField() && IsElementEditable(*input);
  if (is_fillable) {
    focused_input_element_ = *input;
    is_password = focused_input_element_.IsPasswordFieldForAutofill();
  }
  focus_state_notifier_.FocusedInputChanged(is_fillable, is_password);

  if (!web_element.IsFormControlElement())
    return;
  const WebFormControlElement control_element =
      web_element.ToConst<WebFormControlElement>();
  field_data_manager_.UpdateFieldDataMapWithNullValue(
      control_element, FieldPropertiesFlags::HAD_FOCUS);
}

std::unique_ptr<PasswordForm> PasswordAutofillAgent::GetPasswordFormFromWebForm(
    const WebFormElement& web_form) {
  return CreatePasswordFormFromWebForm(web_form, &field_data_manager_,
                                       &form_predictions_,
                                       &username_detector_cache_);
}

std::unique_ptr<PasswordForm>
PasswordAutofillAgent::GetPasswordFormFromUnownedInputElements() {
  // The element's frame might have been detached in the meantime (see
  // http://crbug.com/585363, comments 5 and 6), in which case |frame| will
  // be null. This was hardly caused by form submission (unless the user is
  // supernaturally quick), so it is OK to drop the ball here.
  content::RenderFrame* frame = render_frame();
  if (!frame)
    return nullptr;
  WebLocalFrame* web_frame = frame->GetWebFrame();
  if (!web_frame)
    return nullptr;
  return CreatePasswordFormFromUnownedInputElements(
      *web_frame, &field_data_manager_, &form_predictions_,
      &username_detector_cache_);
}

// mojom::PasswordAutofillAgent:
void PasswordAutofillAgent::SetLoggingState(bool active) {
  logging_state_active_ = active;
}

void PasswordAutofillAgent::AutofillUsernameAndPasswordDataReceived(
    const FormsPredictionsMap& predictions) {
  form_predictions_.insert(predictions.begin(), predictions.end());
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, private:

bool PasswordAutofillAgent::ShowSuggestionPopup(
    const PasswordInfo& password_info,
    const WebInputElement& user_input,
    bool show_all,
    bool show_on_password_field) {
  DCHECK(!user_input.IsNull());
  WebFrame* frame = user_input.GetDocument().GetFrame();
  if (!frame)
    return false;

  WebView* webview = frame->View();
  if (!webview)
    return false;

  if (user_input.IsPasswordFieldForAutofill() && !user_input.IsAutofilled() &&
      !user_input.Value().IsEmpty()) {
    HidePopup();
    return false;
  }

  FormData form;
  FormFieldData field;
  form_util::FindFormAndFieldForFormControlElement(user_input, &form, &field);

  int options = 0;
  if (show_all)
    options |= SHOW_ALL;
  if (show_on_password_field)
    options |= IS_PASSWORD_FIELD;

  base::string16 username_string(user_input.IsPasswordFieldForAutofill()
                                     ? base::string16()
                                     : user_input.Value().Utf16());

  GetPasswordManagerDriver()->ShowPasswordSuggestions(
      field.text_direction, username_string, options,
      render_frame()->GetRenderView()->ElementBoundsInWindow(user_input));
  username_query_prefix_ = username_string;
  return CanShowSuggestion(password_info.fill_data, username_string, show_all);
}

void PasswordAutofillAgent::FrameClosing() {
  web_input_to_password_info_.clear();
  password_to_username_.clear();
  last_supplied_password_info_iter_ = web_input_to_password_info_.end();
  provisionally_saved_form_.Reset();
  field_data_manager_.ClearData();
  username_autofill_state_ = WebAutofillState::kNotFilled;
  password_autofill_state_ = WebAutofillState::kNotFilled;
  sent_request_to_store_ = false;
  checked_safe_browsing_reputation_ = false;
  username_query_prefix_.clear();
  form_predictions_.clear();
  username_detector_cache_.clear();
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  page_passwords_analyser_.Reset();
#endif
}

void PasswordAutofillAgent::ClearPreview(WebInputElement* username,
                                         WebInputElement* password) {
  if (!username->IsNull() && !username->SuggestedValue().IsEmpty()) {
    username->SetSuggestedValue(WebString());
    username->SetAutofillState(username_autofill_state_);
    username->SetSelectionRange(username_query_prefix_.length(),
                                username->Value().length());
  }
  if (!password->SuggestedValue().IsEmpty()) {
    password->SetSuggestedValue(WebString());
    password->SetAutofillState(password_autofill_state_);
  }
}
void PasswordAutofillAgent::ProvisionallySavePassword(
    const WebFormElement& form,
    const WebInputElement& element,
    ProvisionallySaveRestriction restriction) {
  DCHECK(!form.IsNull() || !element.IsNull());

  std::unique_ptr<PasswordForm> password_form;
  if (form.IsNull()) {
    password_form = GetPasswordFormFromUnownedInputElements();
  } else {
    password_form = GetPasswordFormFromWebForm(form);
  }
  if (!password_form)
    return;

  bool has_password = !password_form->password_value.empty() ||
                      !password_form->new_password_value.empty();
  if (restriction == RESTRICTION_NON_EMPTY_PASSWORD && !has_password)
    return;

  if (!FrameCanAccessPasswordManager())
    return;

  provisionally_saved_form_.Set(std::move(password_form), form, element);

  if (has_password) {
    GetPasswordManagerDriver()->ShowManualFallbackForSaving(
        provisionally_saved_form_.password_form());
  } else {
    GetPasswordManagerDriver()->HideManualFallbackForSaving();
  }
}

bool PasswordAutofillAgent::FillUserNameAndPassword(
    WebInputElement* username_element,
    WebInputElement* password_element,
    const PasswordFormFillData& fill_data,
    bool exact_username_match,
    bool username_may_use_prefilled_placeholder,
    FieldDataManager* field_data_manager,
    RendererSavePasswordProgressLogger* logger) {
  if (logger)
    logger->LogMessage(Logger::STRING_FILL_USERNAME_AND_PASSWORD_METHOD);

  // Don't fill username if password can't be set.
  if (!IsElementAutocompletable(*password_element))
    return false;

  // |current_username| is the username for credentials that are going to be
  // autofilled. It is selected according to the algorithm:
  // 1. If the page already contains a non-empty value in |username_element|
  // that is not found in the list of values known to be used as placeholders,
  // this is adopted and not overridden.
  // 2. Default username from |fill_data| if the username field is
  // autocompletable.
  // 3. Empty if username field doesn't exist or if username field is empty and
  // not autocompletable (no username case).
  base::string16 current_username;

  // Whether the username element was prefilled with content that was on a
  // list of known placeholder texts that should be overridden (e.g. "username
  // or email" or there is a server hint that it is just a placeholder).
  bool prefilled_placeholder_username = false;

  if (!username_element->IsNull()) {
    prefilled_placeholder_username =
        !username_element->Value().IsEmpty() &&
        (PossiblePrefilledUsernameValue(username_element->Value().Utf8()) ||
         username_may_use_prefilled_placeholder);
    if (!username_element->Value().IsEmpty() &&
        !prefilled_placeholder_username) {
      // Username is filled with content that was not on a list of known
      // placeholder texts (e.g. "username or email") nor there is server-side
      // data that this value is placeholder.
      current_username = username_element->Value().Utf16();
    } else if (IsElementAutocompletable(*username_element)) {
      current_username = fill_data.username_field.value;
    }
  }

  // |username| and |password| will contain the match found if any.
  base::string16 username;
  base::string16 password;

  FindMatchesByUsername(fill_data, current_username, exact_username_match,
                        logger, &username, &password);

  if (password.empty()) {
    if (!username_element->IsNull() && !username_element->Value().IsEmpty() &&
        !prefilled_placeholder_username) {
      LogPrefilledUsernameFillOutcome(
          PrefilledUsernameFillOutcome::kPrefilledUsernameNotOverridden);
    }
    return false;
  }

  // Call OnFieldAutofilled before WebInputElement::SetAutofillState which may
  // cause frame closing.
  if (password_generation_agent_)
    password_generation_agent_->OnFieldAutofilled(*password_element);

  // Input matches the username, fill in required values.
  if (!username_element->IsNull() &&
      IsElementAutocompletable(*username_element)) {
    if (!username.empty() && (username_element->Value().IsEmpty() ||
                              prefilled_placeholder_username)) {
      username_element->SetSuggestedValue(WebString::FromUTF16(username));
      gatekeeper_.RegisterElement(username_element);
      if (prefilled_placeholder_username) {
        LogPrefilledUsernameFillOutcome(
            PrefilledUsernameFillOutcome::
                kPrefilledPlaceholderUsernameOverridden);
      }
    }
    field_data_manager->UpdateFieldDataMap(
        *username_element, username,
        FieldPropertiesFlags::AUTOFILLED_ON_PAGELOAD);
    username_element->SetAutofillState(WebAutofillState::kAutofilled);
    if (logger)
      logger->LogElementName(Logger::STRING_USERNAME_FILLED, *username_element);
  }

  // Wait to fill in the password until a user gesture occurs. This is to make
  // sure that we do not fill in the DOM with a password until we believe the
  // user is intentionally interacting with the page.
  if (password_element->Value().Utf16() != password)
    password_element->SetSuggestedValue(WebString::FromUTF16(password));
  field_data_manager->UpdateFieldDataMap(
      *password_element, password,
      FieldPropertiesFlags::AUTOFILLED_ON_PAGELOAD);
  ProvisionallySavePassword(password_element->Form(), *password_element,
                            RESTRICTION_NONE);
  gatekeeper_.RegisterElement(password_element);
  password_element->SetAutofillState(WebAutofillState::kAutofilled);

  if (logger)
    logger->LogElementName(Logger::STRING_PASSWORD_FILLED, *password_element);
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

bool PasswordAutofillAgent::FillFormOnPasswordReceived(
    const PasswordFormFillData& fill_data,
    WebInputElement username_element,
    WebInputElement password_element,
    FieldDataManager* field_data_manager,
    RendererSavePasswordProgressLogger* logger) {
  // Do not fill if the password field is in a chain of iframes not having
  // identical origin.
  WebFrame* cur_frame = password_element.GetDocument().GetFrame();
  WebString bottom_frame_origin = cur_frame->GetSecurityOrigin().ToString();

  DCHECK(cur_frame);

  while (cur_frame->Parent()) {
    cur_frame = cur_frame->Parent();
    if (!IsPublicSuffixDomainMatch(
            bottom_frame_origin.Utf8(),
            cur_frame->GetSecurityOrigin().ToString().Utf8()))
      return false;
  }

  // If we can't modify the password, don't try to set the username
  if (!IsElementAutocompletable(password_element))
    return false;

  bool exact_username_match =
      username_element.IsNull() || IsElementEditable(username_element);
  // Use the exact match for the editable username fields and allow prefix
  // match for read-only username fields.
  return FillUserNameAndPassword(
      &username_element, &password_element, fill_data, exact_username_match,
      fill_data.username_may_use_prefilled_placeholder, field_data_manager,
      logger);
}

void PasswordAutofillAgent::OnProvisionallySaveForm(
    const WebFormElement& form,
    const WebFormControlElement& element,
    ElementChangeSource source) {
  // PasswordAutofillAgent isn't interested in SELECT control change.
  if (source == ElementChangeSource::SELECT_CHANGED)
    return;

  WebInputElement input_element;
  if (!element.IsNull() && element.HasHTMLTagName("input"))
    input_element = *ToWebInputElement(&element);

  if (source == ElementChangeSource::TEXTFIELD_CHANGED) {
    DCHECK(!input_element.IsNull());
    // keeps track of all text changes even if it isn't displaying UI.
    UpdateStateForTextChange(input_element);
    return;
  }

  DCHECK_EQ(ElementChangeSource::WILL_SEND_SUBMIT_EVENT, source);
  // Forms submitted via XHR are not seen by WillSubmitForm if the default
  // onsubmit handler is overridden. Such submission first gets detected in
  // DidStartProvisionalLoad, which no longer knows about the particular form,
  // and uses the candidate stored in |provisionally_saved_form_|.
  //
  // User-typed password will get stored to |provisionally_saved_form_| in
  // TextDidChangeInTextField. Autofilled or JavaScript-copied passwords need to
  // be saved here.
  //
  // Only non-empty passwords are saved here. Empty passwords were likely
  // cleared by some scripts (http://crbug.com/28910, http://crbug.com/391693).
  // Had the user cleared the password, |provisionally_saved_form_| would
  // already have been updated in TextDidChangeInTextField.
  ProvisionallySavePassword(form, input_element,
                            RESTRICTION_NON_EMPTY_PASSWORD);
}

void PasswordAutofillAgent::OnFormSubmitted(const WebFormElement& form) {
  OnWillSubmitForm(form);
}

void PasswordAutofillAgent::OnInferredFormSubmission(SubmissionSource source) {
  if (source == SubmissionSource::FRAME_DETACHED) {
    OnFrameDetached();
  } else {
    PasswordForm::SubmissionIndicatorEvent event =
        ToSubmissionIndicatorEvent(source);
    if (event == PasswordForm::SubmissionIndicatorEvent::NONE)
      return;
    FireSubmissionIfFormDisappear(event);
  }
}

void PasswordAutofillAgent::HidePopup() {
  AutofillAgent* agent = autofill_agent_.get();
  if (agent) {
    autofill_agent_->GetAutofillDriver()->HidePopup();
  }
}

const mojom::PasswordManagerDriverAssociatedPtr&
PasswordAutofillAgent::GetPasswordManagerDriver() {
  if (!password_manager_driver_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &password_manager_driver_);
  }
  return password_manager_driver_;
}

std::pair<WebInputElement, WebInputElement>
PasswordAutofillAgent::FindUsernamePasswordElements(
    const PasswordFormFillData& form_data) {
  const uint32_t username_renderer_id =
      form_data.username_field.unique_renderer_id;
  const uint32_t password_renderer_id =
      form_data.password_field.unique_renderer_id;
  const bool is_username_present =
      username_renderer_id != FormFieldData::kNotSetFormControlRendererId;
  const bool is_password_present =
      password_renderer_id != FormFieldData::kNotSetFormControlRendererId;

  std::vector<uint32_t> element_ids;
  if (is_password_present)
    element_ids.push_back(password_renderer_id);
  if (is_username_present)
    element_ids.push_back(username_renderer_id);

  WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
  bool wrapped_in_form_tag =
      form_data.form_renderer_id != FormData::kNotSetFormRendererId;
  std::vector<WebFormControlElement> elements =
      wrapped_in_form_tag
          ? form_util::FindFormControlElementsByUniqueRendererId(
                doc, form_data.form_renderer_id, element_ids)
          : form_util::FindFormControlElementsByUniqueRendererId(doc,
                                                                 element_ids);

  // Set password element.
  WebInputElement password_field;
  size_t current_index = 0;
  if (is_password_present)
    password_field = ConvertToWebInput(elements[current_index++]);

  // Set username element.
  WebInputElement username_field;
  if (is_username_present)
    username_field = ConvertToWebInput(elements[current_index++]);

  return std::make_pair(username_field, password_field);
}

void PasswordAutofillAgent::StoreDataForFillOnAccountSelect(
    const PasswordFormFillData& form_data,
    WebInputElement username_element,
    WebInputElement password_element) {
  WebInputElement main_element =
      username_element.IsNull() ? password_element : username_element;

  PasswordInfo password_info;
  password_info.fill_data = form_data;
  password_info.password_field = password_element;
  web_input_to_password_info_[main_element] = password_info;
  last_supplied_password_info_iter_ =
      web_input_to_password_info_.find(main_element);
  if (!main_element.IsPasswordFieldForAutofill())
    password_to_username_[password_element] = username_element;
}

void PasswordAutofillAgent::MaybeStoreFallbackData(
    const PasswordFormFillData& form_data) {
  if (!web_input_to_password_info_.empty())
    return;
  // If for some reasons elements for filling were not found (for example
  // because they were renamed by JavaScript) then add fill data for
  // |web_input_to_password_info_|. When the user clicks on a password field
  // which is not a key in |web_input_to_password_info_|, the first element from
  // |web_input_to_password_info_| will be used in
  // PasswordAutofillAgent::FindPasswordInfoForElement to propose to fill.
  PasswordInfo password_info;
  password_info.fill_data = form_data;
  web_input_to_password_info_[WebInputElement()] = password_info;
  last_supplied_password_info_iter_ = web_input_to_password_info_.begin();
}

}  // namespace autofill
