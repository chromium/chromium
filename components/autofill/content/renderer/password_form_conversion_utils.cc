// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <string>

#include "base/i18n/case_conversion.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebString;

namespace autofill {

namespace {

constexpr char kAutocompleteUsername[] = "username";
constexpr char kAutocompleteCurrentPassword[] = "current-password";
constexpr char kAutocompleteNewPassword[] = "new-password";
constexpr char kAutocompleteCreditCardPrefix[] = "cc-";

// Parses the string with the value of an autocomplete attribute. If any of the
// tokens "username", "current-password" or "new-password" are present, returns
// an appropriate enum value, picking an arbitrary one if more are applicable.
// Otherwise, it returns CREDIT_CARD if a token with a "cc-" prefix is found.
// Otherwise, returns NONE.
AutocompleteFlag ExtractAutocompleteFlag(const std::string& attribute) {
  std::vector<base::StringPiece> tokens =
      base::SplitStringPiece(attribute, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  bool cc_seen = false;
  for (base::StringPiece token : tokens) {
    if (base::LowerCaseEqualsASCII(token, kAutocompleteUsername))
      return AutocompleteFlag::USERNAME;
    if (base::LowerCaseEqualsASCII(token, kAutocompleteCurrentPassword))
      return AutocompleteFlag::CURRENT_PASSWORD;
    if (base::LowerCaseEqualsASCII(token, kAutocompleteNewPassword))
      return AutocompleteFlag::NEW_PASSWORD;

    if (!cc_seen) {
      cc_seen = base::StartsWith(token, kAutocompleteCreditCardPrefix,
                                 base::CompareCase::SENSITIVE);
    }
  }
  return cc_seen ? AutocompleteFlag::CREDIT_CARD : AutocompleteFlag::NONE;
}

// Helper to spare map::find boilerplate when caching field's autocomplete
// attributes.
class AutocompleteCache {
 public:
  AutocompleteCache();

  ~AutocompleteCache();

  // Computes and stores the AutocompleteFlag for |field| based on its
  // autocomplete attribute. Note that this cannot be done on-demand during
  // RetrieveFor, because the cache spares space and look-up time by not storing
  // AutocompleteFlag::NONE values, hence for all elements without an
  // autocomplete attribute, every retrieval would result in a new computation.
  void Store(const FormFieldData* field);

  // Retrieves the value previously stored for |field|.
  AutocompleteFlag RetrieveFor(const FormFieldData* field) const;

 private:
  std::map<const FormFieldData*, AutocompleteFlag> cache_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteCache);
};

AutocompleteCache::AutocompleteCache() = default;

AutocompleteCache::~AutocompleteCache() = default;

void AutocompleteCache::Store(const FormFieldData* field) {
  const AutocompleteFlag flag =
      ExtractAutocompleteFlag(field->autocomplete_attribute);
  // Only store non-trivial flags. Most of the elements will have the NONE
  // value, so spare storage and lookup time by assuming anything not stored in
  // |cache_| has the NONE flag.
  if (flag != AutocompleteFlag::NONE)
    cache_[field] = flag;
}

AutocompleteFlag AutocompleteCache::RetrieveFor(
    const FormFieldData* field) const {
  auto it = cache_.find(field);
  if (it == cache_.end())
    return AutocompleteFlag::NONE;
  return it->second;
}

// Describes fields filtering criteria. More priority criteria has higher value
// in the enum. The fields with the maximal criteria are considered in a form,
// others are ignored. Criteria for password and username fields are calculated
// separately. For example, if there is a password field with user input, the
// password fields without user input are ignored (independently whether the
// fields are visible or not).
enum class FieldFilteringLevel {
  NO_FILTER = 0,
  VISIBILITY = 1,
  USER_INPUT = 2
};

// Helper to determine which password is the main (current) one, and which is
// the new password (e.g., on a sign-up or change password form), if any. If the
// new password is found and there is another password field with the same user
// input, the function also sets |confirmation_password| to this field.
void LocateSpecificPasswords(std::vector<const FormFieldData*> passwords,
                             const FormFieldData** current_password,
                             const FormFieldData** new_password,
                             const FormFieldData** confirmation_password,
                             const AutocompleteCache& autocomplete_cache) {
  DCHECK(!passwords.empty());
  DCHECK(current_password && !*current_password);
  DCHECK(new_password && !*new_password);
  DCHECK(confirmation_password && !*confirmation_password);

  // First, look for elements marked with either autocomplete='current-password'
  // or 'new-password' -- if we find any, take the hint, and treat the first of
  // each kind as the element we are looking for.
  for (const FormFieldData* password : passwords) {
    const AutocompleteFlag flag = autocomplete_cache.RetrieveFor(password);
    if (flag == AutocompleteFlag::CURRENT_PASSWORD && !*current_password) {
      *current_password = password;
    } else if (flag == AutocompleteFlag::NEW_PASSWORD && !*new_password) {
      *new_password = password;
    } else if (*new_password && ((*new_password)->value == password->value)) {
      *confirmation_password = password;
    }
  }

  // If we have seen an element with either of autocomplete attributes above,
  // take that as a signal that the page author must have intentionally left the
  // rest of the password fields unmarked. Perhaps they are used for other
  // purposes, e.g., PINs, OTPs, and the like. So we skip all the heuristics we
  // normally do, and ignore the rest of the password fields.
  if (*current_password || *new_password)
    return;

  switch (passwords.size()) {
    case 1:
      // Single password, easy.
      *current_password = passwords[0];
      break;
    case 2:
      if (!passwords[0]->value.empty() &&
          passwords[0]->value == passwords[1]->value) {
        // Two identical non-empty passwords: assume we are seeing a new
        // password with a confirmation. This can be either a sign-up form or a
        // password change form that does not ask for the old password.
        *new_password = passwords[0];
        *confirmation_password = passwords[1];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        // This case also includes empty passwords in order to allow filling of
        // password change forms (that also could autofill for sign up form, but
        // we can't do anything with this using only client side information).
        *current_password = passwords[0];
        *new_password = passwords[1];
      }
      break;
    default:
      if (!passwords[0]->value.empty() &&
          passwords[0]->value == passwords[1]->value &&
          passwords[0]->value == passwords[2]->value) {
        // All three passwords are the same and non-empty? It may be a change
        // password form where old and new passwords are the same. It doesn't
        // matter what field is correct, let's save the value.
        *current_password = passwords[0];
      } else if (passwords[1]->value == passwords[2]->value) {
        // New password is the duplicated one, and comes second; or empty form
        // with 3 password fields, in which case we will assume this layout.
        *current_password = passwords[0];
        *new_password = passwords[1];
        *confirmation_password = passwords[2];
      } else if (passwords[0]->value == passwords[1]->value) {
        // It is strange that the new password comes first, but trust more which
        // fields are duplicated than the ordering of fields. Assume that
        // any password fields after the new password contain sensitive
        // information that isn't actually a password (security hint, SSN, etc.)
        *new_password = passwords[0];
        *confirmation_password = passwords[1];
      } else {
        // Three different passwords, or first and last match with middle
        // different. No idea which is which. Let's save the first password.
        // Password selection in a prompt will allow to correct the choice.
        *current_password = passwords[0];
      }
  }
}

void FindPredictedElements(
    const FormData& form_data,
    const FormsPredictionsMap& form_predictions,
    std::map<const FormFieldData*, PasswordFormFieldPredictionType>*
        predicted_fields) {
  // Matching only requires that action and name of the form match to allow
  // the username to be updated even if the form is changed after page load.
  // See https://crbug.com/476092 for more details.
  const PasswordFormFieldPredictionMap* field_predictions = nullptr;
  for (const auto& form_predictions_pair : form_predictions) {
    if (form_predictions_pair.first.action == form_data.action &&
        form_predictions_pair.first.name == form_data.name) {
      field_predictions = &form_predictions_pair.second;
      break;
    }
  }

  if (!field_predictions)
    return;

  for (const auto& prediction : *field_predictions) {
    const FormFieldData& target_field = prediction.first;
    const PasswordFormFieldPredictionType& type = prediction.second;
    for (const FormFieldData& field : form_data.fields) {
      if (field.name == target_field.name) {
        (*predicted_fields)[&field] = type;
        break;
      }
    }
  }
}

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

// Returns the |input_field| name if its non-empty; otherwise a |dummy_name|.
base::string16 FieldName(const FormFieldData* input_field,
                         const char* dummy_name) {
  return input_field->name.empty() ? base::ASCIIToUTF16(dummy_name)
                                   : input_field->name;
}

// Return the maximal filtering criteria that |field| passes.
// If |ignore_autofilled_values|, autofilled value isn't considered user input.
FieldFilteringLevel GetFiltertingLevelForField(const FormFieldData& field,
                                               bool ignore_autofilled_values) {
  FieldPropertiesMask user_input_mask =
      ignore_autofilled_values
          ? FieldPropertiesFlags::USER_TYPED
          : FieldPropertiesFlags::USER_TYPED | FieldPropertiesFlags::AUTOFILLED;
  if (field.properties_mask & user_input_mask)
    return FieldFilteringLevel::USER_INPUT;
  return field.is_focusable ? FieldFilteringLevel::VISIBILITY
                            : FieldFilteringLevel::NO_FILTER;
}

// Calculates the maximal filtering levels for password and username fields and
// saves them to |username_fields_level| and |password_fields_level|. The
// criteria for username fields considers only the fields before the first
// password field that has the maximal filtering level.
void GetFieldFilteringLevels(const std::vector<FormFieldData>& fields,
                             FieldFilteringLevel* username_fields_level,
                             FieldFilteringLevel* password_fields_level) {
  DCHECK(password_fields_level);
  DCHECK(username_fields_level);
  *username_fields_level = FieldFilteringLevel::NO_FILTER;
  *password_fields_level = FieldFilteringLevel::NO_FILTER;

  FieldFilteringLevel max_level_found_for_username_fields =
      FieldFilteringLevel::NO_FILTER;
  for (const FormFieldData& field : fields) {
    if (!field.is_enabled || !field.IsTextInputElement())
      continue;

    // TODO(crbug.com/789917): Ignore autofilled values here because if there
    // are only autofilled values then a form may not be filled completely (i.e.
    // some user input is still expected). So, user input shouldn't be used for
    // fields filtering. Once the bug is resolved, autofilled values will not be
    // ignored.
    FieldFilteringLevel current_field_filtering_level =
        GetFiltertingLevelForField(field, true /* ignore_autofilled_values */);

    if (field.form_control_type == "password") {
      if (*password_fields_level < current_field_filtering_level) {
        *password_fields_level = current_field_filtering_level;
        *username_fields_level = max_level_found_for_username_fields;
      }
    } else {
      max_level_found_for_username_fields = std::max(
          max_level_found_for_username_fields, current_field_filtering_level);
    }
  }
}

ValueElementPair MakePossibleUsernamePair(const FormFieldData* input) {
  base::string16 trimmed_input_value;
  base::TrimString(input->value, base::ASCIIToUTF16(" "), &trimmed_input_value);
  return {trimmed_input_value, input->name};
}

bool StringMatchesCVC(const base::string16& str) {
  static const base::NoDestructor<base::string16> kCardCvcReCached(
      base::UTF8ToUTF16(kCardCvcRe));

  return MatchesPattern(str, *kCardCvcReCached);
}

bool IsEnabledPasswordFieldPresent(const std::vector<FormFieldData>& fields) {
  return std::find_if(
             fields.begin(), fields.end(), [](const FormFieldData& field) {
               return field.is_enabled && field.form_control_type == "password";
             }) != fields.end();
}

// Find the first element in |username_predictions| (i.e. the most reliable
// prediction) that occurs in |possible_usernames|.
const FormFieldData* FindUsernameInPredictions(
    const std::vector<uint32_t>& username_predictions,
    const std::vector<const FormFieldData*>& possible_usernames) {
  for (uint32_t predicted_id : username_predictions) {
    auto iter =
        std::find_if(possible_usernames.begin(), possible_usernames.end(),
                     [predicted_id](const FormFieldData* field) {
                       return field->unique_renderer_id == predicted_id;
                     });
    if (iter != possible_usernames.end()) {
      return *iter;
    }
  }
  return nullptr;
}

// Extracts the username predictions. |control_elements| should be all the DOM
// elements of the form, |form_data| should be the already extracted FormData
// representation of that form. |username_detector_cache| is optional, and can
// be used to spare recomputation if called multiple times for the same form.
std::vector<uint32_t> GetUsernamePredictions(
    const std::vector<blink::WebFormControlElement>& control_elements,
    const FormData& form_data,
    UsernameDetectorCache* username_detector_cache) {
  std::vector<uint32_t> username_predictions;
  // Dummy cache stores the predictions in case no real cache was passed to
  // here.
  UsernameDetectorCache dummy_cache;
  if (!username_detector_cache)
    username_detector_cache = &dummy_cache;

  const std::vector<WebInputElement>& username_predictions_dom =
      GetPredictionsFieldBasedOnHtmlAttributes(control_elements, form_data,
                                               username_detector_cache);
  username_predictions.reserve(username_predictions_dom.size());
  for (const WebInputElement& element : username_predictions_dom) {
    username_predictions.push_back(element.UniqueRendererFormControlId());
  }
  return username_predictions;
}

// Get information about a login form encapsulated in a PasswordForm struct.
// If an element of |form| has an entry in |nonscript_modified_values|, the
// associated string is used instead of the element's value to create
// the PasswordForm.
bool GetPasswordForm(
    const GURL& form_origin,
    const std::vector<blink::WebFormControlElement>& control_elements,
    PasswordForm* password_form,
    const FormsPredictionsMap* form_predictions,
    UsernameDetectorCache* username_detector_cache) {
  DCHECK(!control_elements.empty());

  const FormData& form_data = password_form->form_data;

  // Early exit if no passwords to be typed into.
  if (!IsEnabledPasswordFieldPresent(form_data.fields))
    return false;

  // Evaluate the context of the fields.
  if (base::FeatureList::IsEnabled(
          password_manager::features::kHtmlBasedUsernameDetector)) {
    password_form->form_data.username_predictions = GetUsernamePredictions(
        control_elements, form_data, username_detector_cache);
  }

  // Narrow the scope to enabled text inputs.
  std::vector<const FormFieldData*> enabled_fields;
  enabled_fields.reserve(form_data.fields.size());
  for (const FormFieldData& field : form_data.fields) {
    if (field.is_enabled && field.IsTextInputElement())
      enabled_fields.push_back(&field);
  }

  // Remember the list of password fields without any heuristics applied in case
  // the heuristics fail and a fall-back is needed:
  // All password fields.
  std::vector<const FormFieldData*> passwords_without_heuristics;
  // Map from all password fields to the most recent non-password text input.
  std::map<const FormFieldData*, const FormFieldData*>
      preceding_text_input_for_password_without_heuristics;
  const FormFieldData* most_recent_text_input = nullptr;  // Just a temporary.
  for (const FormFieldData* input : enabled_fields) {
    if (input->form_control_type == "password") {
      passwords_without_heuristics.push_back(input);
      preceding_text_input_for_password_without_heuristics[input] =
          most_recent_text_input;
    } else {
      most_recent_text_input = input;
    }
  }

  // Fill the cache with autocomplete flags.
  AutocompleteCache autocomplete_cache;
  for (const FormFieldData* input : enabled_fields) {
    autocomplete_cache.Store(input);
  }

  // Narrow the scope further: drop credit-card fields.
  std::vector<const FormFieldData*> plausible_inputs;
  plausible_inputs.reserve(enabled_fields.size());
  for (const FormFieldData* input : enabled_fields) {
    const AutocompleteFlag flag = autocomplete_cache.RetrieveFor(input);
    if (flag == AutocompleteFlag::CURRENT_PASSWORD ||
        flag == AutocompleteFlag::NEW_PASSWORD) {
      // A field marked as a password is considered not a credit-card field, no
      // matter what.
      plausible_inputs.push_back(input);
    } else if (flag != AutocompleteFlag::CREDIT_CARD) {
      const bool is_credit_card_verification =
          input->form_control_type == "password" &&
          (StringMatchesCVC(input->name) || StringMatchesCVC(input->id));
      if (!is_credit_card_verification) {
        // Otherwise ensure that nothing hints that |input| is a credit-card
        // field.
        plausible_inputs.push_back(input);
      }
    }
  }

  // Further narrow to interesting fields (e.g., with user input, visible), if
  // present.
  // Compute the best filtering levels for usernames and for passwords.
  FieldFilteringLevel username_fields_level = FieldFilteringLevel::NO_FILTER;
  FieldFilteringLevel password_fields_level = FieldFilteringLevel::NO_FILTER;
  GetFieldFilteringLevels(form_data.fields, &username_fields_level,
                          &password_fields_level);
  // Remove all fields with filtering level below the best.
  base::EraseIf(
      plausible_inputs, [password_fields_level,
                         username_fields_level](const FormFieldData* input) {
        FieldFilteringLevel current_field_level = GetFiltertingLevelForField(
            *input, false /* ignore_autofilled_values */);
        if (input->form_control_type == "password")
          return current_field_level < password_fields_level;
        return current_field_level < username_fields_level;
      });

  // Further, remove all readonly passwords. If the password field is readonly,
  // the page is likely using a virtual keyboard and bypassing the password
  // field value (see http://crbug.com/475488). There is nothing Chrome can do
  // to fill passwords for now. Notable exceptions: if the password field was
  // made readonly by JavaScript before submission, it remains interesting. If
  // the password was marked via the autocomplete attribute, it also remains
  // interesting.
  base::EraseIf(plausible_inputs, [&autocomplete_cache](
                                      const FormFieldData* input) {
    if (!input->is_readonly)
      return false;
    if (input->form_control_type != "password")
      return false;
    // Check if the field was only made readonly before submission.
    if (input->properties_mask &
        (FieldPropertiesFlags::USER_TYPED | FieldPropertiesFlags::AUTOFILLED)) {
      return false;
    }
    // Check whether the field was explicitly marked as password.
    const AutocompleteFlag flag = autocomplete_cache.RetrieveFor(input);
    if (flag == AutocompleteFlag::CURRENT_PASSWORD ||
        flag == AutocompleteFlag::NEW_PASSWORD) {
      return false;
    }
    return true;
  });

  // Evaluate available server-side predictions.
  std::map<const FormFieldData*, PasswordFormFieldPredictionType>
      predicted_fields;
  const FormFieldData* predicted_username_field = nullptr;
  if (form_predictions) {
    FindPredictedElements(password_form->form_data, *form_predictions,
                          &predicted_fields);

    for (const auto& predicted_pair : predicted_fields) {
      if (predicted_pair.second == PREDICTION_USERNAME) {
        predicted_username_field = predicted_pair.first;
        break;
      }
    }
  }

  // Finally, remove all password fields for which we have a negative
  // prediction, unless they are explicitly marked by the autocomplete attribute
  // as a password.
  base::EraseIf(plausible_inputs, [&predicted_fields, &autocomplete_cache](
                                      const FormFieldData* input) {
    if (input->form_control_type != "password")
      return false;
    const AutocompleteFlag flag = autocomplete_cache.RetrieveFor(input);
    if (flag == AutocompleteFlag::CURRENT_PASSWORD ||
        flag == AutocompleteFlag::NEW_PASSWORD) {
      return false;
    }
    auto possible_password_field_iterator = predicted_fields.find(input);
    return possible_password_field_iterator != predicted_fields.end() &&
           possible_password_field_iterator->second == PREDICTION_NOT_PASSWORD;
  });

  // Derive the list of all plausible passwords, usernames and the non-password
  // inputs preceding the plausible passwords.
  std::vector<const FormFieldData*> plausible_passwords;
  std::vector<const FormFieldData*> plausible_usernames;
  std::map<const FormFieldData*, const FormFieldData*>
      preceding_text_input_for_plausible_password;
  most_recent_text_input = nullptr;
  plausible_usernames.reserve(plausible_inputs.size());
  for (const FormFieldData* input : plausible_inputs) {
    if (input->form_control_type == "password") {
      plausible_passwords.push_back(input);
      preceding_text_input_for_plausible_password[input] =
          most_recent_text_input;
    } else {
      plausible_usernames.push_back(input);
      most_recent_text_input = input;
    }
  }

  // Evaluate autocomplete attributes for username.
  const FormFieldData* username_by_attribute = nullptr;
  for (const FormFieldData* input : plausible_inputs) {
    if (input->form_control_type != "password") {
      if (autocomplete_cache.RetrieveFor(input) == AutocompleteFlag::USERNAME) {
        // Only consider the first occurrence of autocomplete='username'.
        // Multiple occurences hint at the attribute being used incorrectly, in
        // which case sticking to the first one is just a bet.
        if (!username_by_attribute) {
          username_by_attribute = input;
          break;
        }
      }
    }
  }

  // Evaluate the context of the fields.
  const FormFieldData* username_field_by_context = nullptr;
  if (base::FeatureList::IsEnabled(
          password_manager::features::kHtmlBasedUsernameDetector)) {
    // Use HTML based username detector only if neither server predictions nor
    // autocomplete attributes were useful to detect the username.
    if (!predicted_username_field && !username_by_attribute) {
      username_field_by_context = FindUsernameInPredictions(
          form_data.username_predictions, plausible_usernames);
    }
  }

  // Populate all_possible_passwords and form_has_autofilled_value in
  // |password_form|.
  // Contains the first password element for each non-empty password value.
  std::vector<ValueElementPair> all_possible_passwords;
  // Reserve enough space to prevent re-allocation. A re-allocation would
  // invalidate the contents of |seen_values|.
  all_possible_passwords.reserve(passwords_without_heuristics.size());
  std::set<base::StringPiece16> seen_values;
  // Pretend that an empty value has been already seen, so that empty-valued
  // password elements won't get added to |all_possible_passwords|.
  seen_values.insert(base::StringPiece16());
  for (const FormFieldData* password_field : passwords_without_heuristics) {
    if (seen_values.count(password_field->value) > 0)
      continue;
    all_possible_passwords.push_back(
        {password_field->value, password_field->name});
    seen_values.insert(
        base::StringPiece16(all_possible_passwords.back().first));
  }

  bool form_has_autofilled_value = false;
  for (const FormFieldData* password_field : passwords_without_heuristics) {
    bool field_has_autofilled_value =
        password_field->properties_mask & FieldPropertiesFlags::AUTOFILLED;
    form_has_autofilled_value |= field_has_autofilled_value;
  }

  if (!all_possible_passwords.empty()) {
    password_form->all_possible_passwords = std::move(all_possible_passwords);
    password_form->form_has_autofilled_value = form_has_autofilled_value;
  }

  // If for some reason (e.g. only credit card fields, confusing autocomplete
  // attributes) the passwords list is empty, build list based on user input (if
  // there is any non-empty password field) and the type of a field. Also mark
  // that the form should be available only for fallback saving (automatic
  // bubble will not pop up).
  password_form->only_for_fallback_saving = plausible_passwords.empty();
  if (plausible_passwords.empty()) {
    plausible_passwords = std::move(passwords_without_heuristics);
    preceding_text_input_for_plausible_password =
        std::move(preceding_text_input_for_password_without_heuristics);
  }

  // Find the password fields.
  const FormFieldData* password = nullptr;
  const FormFieldData* new_password = nullptr;
  const FormFieldData* confirmation_password = nullptr;
  LocateSpecificPasswords(std::move(plausible_passwords), &password,
                          &new_password, &confirmation_password,
                          autocomplete_cache);

  // Choose the username element.
  const FormFieldData* username_field = nullptr;
  UsernameDetectionMethod username_detection_method =
      UsernameDetectionMethod::NO_USERNAME_DETECTED;
  password_form->username_marked_by_site = false;

  if (predicted_username_field) {
    // Server predictions are most trusted, so try them first. Only if the form
    // already has user input and the predicted username field has an empty
    // value, then don't trust the prediction (can be caused by, e.g., a <form>
    // actually contains several forms).
    if (password_fields_level < FieldFilteringLevel::USER_INPUT ||
        !predicted_username_field->value.empty()) {
      username_field = predicted_username_field;
      password_form->was_parsed_using_autofill_predictions = true;
      username_detection_method =
          UsernameDetectionMethod::SERVER_SIDE_PREDICTION;
    }
  }

  if (!username_field && username_by_attribute) {
    // Next in the trusted queue: autocomplete attributes.
    username_field = username_by_attribute;
    username_detection_method = UsernameDetectionMethod::AUTOCOMPLETE_ATTRIBUTE;
  }

  if (!username_field && username_field_by_context) {
    // Last step before base heuristics: HTML-based classifier.
    username_field = username_field_by_context;
    username_detection_method = UsernameDetectionMethod::HTML_BASED_CLASSIFIER;
  }

  // Compute base heuristic for username detection.
  const FormFieldData* base_heuristic_username = nullptr;
  if (password) {
    base_heuristic_username =
        preceding_text_input_for_plausible_password[password];
  }
  if (!base_heuristic_username && new_password) {
    base_heuristic_username =
        preceding_text_input_for_plausible_password[new_password];
  }

  // Apply base heuristic for username detection.
  if (!username_field) {
    username_field = base_heuristic_username;
    if (username_field)
      username_detection_method = UsernameDetectionMethod::BASE_HEURISTIC;
  } else if (base_heuristic_username == username_field &&
             username_detection_method !=
                 UsernameDetectionMethod::AUTOCOMPLETE_ATTRIBUTE) {
    // TODO(crbug.com/786404): when the bug is fixed, remove this block and
    // calculate |base_heuristic_username| only if |username_field| is null.
    // This block was added to measure the impact of server-side predictions and
    // HTML based classifier compared to "old classifiers" (the based heuristic
    // and 'autocomplete' attribute).
    username_detection_method = UsernameDetectionMethod::BASE_HEURISTIC;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.UsernameDetectionMethod", username_detection_method,
      UsernameDetectionMethod::USERNAME_DETECTION_METHOD_COUNT);

  // Populate the username fields in |password_form|.
  if (username_field) {
    password_form->username_element =
        FieldName(username_field, "anonymous_username");
    password_form->username_value = username_field->value;
    if ((username_field->properties_mask &
         (FieldPropertiesFlags::USER_TYPED |
          FieldPropertiesFlags::AUTOFILLED)) &&
        !username_field->typed_value.empty()) {
      password_form->username_value = username_field->typed_value;
    }
  }

  // Populate the password fields in |password_form|.
  if (password) {
    password_form->password_element = FieldName(password, "anonymous_password");
    password_form->password_value = password->value;
    if ((password->properties_mask & (FieldPropertiesFlags::USER_TYPED |
                                      FieldPropertiesFlags::AUTOFILLED)) &&
        !password->typed_value.empty()) {
      password_form->password_value = password->typed_value;
    }
  }
  if (new_password) {
    password_form->new_password_element =
        FieldName(new_password, "anonymous_new_password");
    password_form->new_password_value = new_password->value;
    if (autocomplete_cache.RetrieveFor(new_password) ==
        AutocompleteFlag::NEW_PASSWORD) {
      password_form->new_password_marked_by_site = true;
    }
    if (confirmation_password) {
      password_form->confirmation_password_element =
          FieldName(confirmation_password, "anonymous_confirmation_password");
    }
  }

  // Populate |other_possible_usernames| in |password_form|.
  ValueElementVector other_possible_usernames;
  for (const FormFieldData* plausible_username : plausible_usernames) {
    if (plausible_username == username_field)
      continue;
    auto pair = MakePossibleUsernamePair(plausible_username);
    if (!pair.first.empty())
      other_possible_usernames.push_back(std::move(pair));
  }
  password_form->other_possible_usernames = std::move(other_possible_usernames);

  password_form->origin = std::move(form_origin);
  password_form->signon_realm = GetSignOnRealm(password_form->origin);
  password_form->scheme = PasswordForm::SCHEME_HTML;
  password_form->preferred = false;
  password_form->blacklisted_by_user = false;
  password_form->type = PasswordForm::TYPE_MANUAL;

  return true;
}

bool HasGaiaSchemeAndHost(const WebFormElement& form) {
  GURL form_url = form.GetDocument().Url();
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  return form_url.scheme() == gaia_url.scheme() &&
         form_url.host() == gaia_url.host();
}

}  // namespace

AutocompleteFlag AutocompleteFlagForElement(const WebInputElement& element) {
  static const base::NoDestructor<WebString> kAutocomplete(("autocomplete"));
  return ExtractAutocompleteFlag(
      element.GetAttribute(*kAutocomplete)
          .Utf8(WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD));
}

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
  if (!HasGaiaSchemeAndHost(form))
    return false;

  bool has_rart_field = false;
  bool has_continue_field = false;

  blink::WebVector<blink::WebFormControlElement> web_control_elements;
  form.GetFormControlElements(web_control_elements);

  for (const blink::WebFormControlElement& element : web_control_elements) {
    // We're only interested in the presence
    // of <input type="hidden" /> elements.
    static base::NoDestructor<WebString> kHidden("hidden");
    const blink::WebInputElement* input = blink::ToWebInputElement(&element);
    if (!input || input->FormControlTypeForAutofill() != *kHidden)
      continue;

    // There must be a hidden input named "rart".
    if (input->FormControlName() == "rart")
      has_rart_field = true;

    // There must be a hidden input named "continue", whose value points
    // to a password (or password testing) site.
    if (input->FormControlName() == "continue" &&
        re2::RE2::PartialMatch(input->Value().Utf8(),
                               g_password_site_matcher.Get())) {
      has_continue_field = true;
    }
  }
  return has_rart_field && has_continue_field;
}

bool IsGaiaWithSkipSavePasswordForm(const blink::WebFormElement& form) {
  if (!HasGaiaSchemeAndHost(form))
    return false;

  GURL url(form.GetDocument().Url());
  std::string should_skip_password;
  if (!net::GetValueForKeyInQuery(url, "ssp", &should_skip_password))
    return false;
  return should_skip_password == "1";
}

std::unique_ptr<PasswordForm> CreatePasswordFormFromWebForm(
    const WebFormElement& web_form,
    const FieldDataManager* field_data_manager,
    const FormsPredictionsMap* form_predictions,
    UsernameDetectorCache* username_detector_cache) {
  if (web_form.IsNull())
    return nullptr;

  auto password_form = std::make_unique<PasswordForm>();
  password_form->action = form_util::GetCanonicalActionForForm(web_form);
  if (!password_form->action.is_valid())
    return nullptr;
  password_form->is_gaia_with_skip_save_password_form =
      IsGaiaWithSkipSavePasswordForm(web_form) ||
      IsGaiaReauthenticationForm(web_form);

  blink::WebVector<blink::WebFormControlElement> control_elements;
  web_form.GetFormControlElements(control_elements);
  if (control_elements.empty())
    return nullptr;

  if (!WebFormElementToFormData(web_form, blink::WebFormControlElement(),
                                field_data_manager, form_util::EXTRACT_VALUE,
                                &password_form->form_data,
                                nullptr /* FormFieldData */)) {
    return nullptr;
  }

  if (!GetPasswordForm(
          form_util::GetCanonicalOriginForDocument(web_form.GetDocument()),
          control_elements.ReleaseVector(), password_form.get(),
          form_predictions, username_detector_cache)) {
    return nullptr;
  }
  return password_form;
}

std::unique_ptr<PasswordForm> CreatePasswordFormFromUnownedInputElements(
    const WebLocalFrame& frame,
    const FieldDataManager* field_data_manager,
    const FormsPredictionsMap* form_predictions,
    UsernameDetectorCache* username_detector_cache) {
  std::vector<blink::WebElement> fieldsets;
  std::vector<blink::WebFormControlElement> control_elements =
      form_util::GetUnownedFormFieldElements(frame.GetDocument().All(),
                                             &fieldsets);
  if (control_elements.empty())
    return nullptr;

  auto password_form = std::make_unique<PasswordForm>();
  if (!UnownedPasswordFormElementsAndFieldSetsToFormData(
          fieldsets, control_elements, nullptr, frame.GetDocument(),
          field_data_manager, form_util::EXTRACT_VALUE,
          &password_form->form_data, nullptr /* FormFieldData */)) {
    return nullptr;
  }

  if (!GetPasswordForm(
          form_util::GetCanonicalOriginForDocument(frame.GetDocument()),
          control_elements, password_form.get(), form_predictions,
          username_detector_cache)) {
    return nullptr;
  }

  // No actual action on the form, so use the the origin as the action.
  password_form->action = password_form->origin;
  return password_form;
}

std::string GetSignOnRealm(const GURL& origin) {
  GURL::Replacements rep;
  rep.SetPathStr("");
  return origin.ReplaceComponents(rep).spec();
}

}  // namespace autofill
