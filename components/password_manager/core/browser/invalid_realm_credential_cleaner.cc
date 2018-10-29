// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/invalid_realm_credential_cleaner.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "base/strings/string_piece.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

using FormVector = std::vector<std::unique_ptr<autofill::PasswordForm>>;

// This type defines a subset of PasswordForm where the first argument is the
// date_created, the second argument is the origin (excluding protocol), and the
// third argument is the username of the form.
// Excluding protocol from an url means, for example, that "http://google.com/"
// after excluding protocol becomes "google.com/".
using FormKeyForHttpMatch = std::tuple<base::Time, std::string, base::string16>;

// This type defines a subset of PasswordForm where the first argument is
// the web origin (given by origin.GetOrigin().spec()) and the second
// argument is the username of the form.
using FormKeyForHttpsMatch = std::pair<std::string, base::string16>;

// This function moves the elements from |forms| for which predicate |p| is true
// to |.first| and the rest of elements to |.second|.
template <typename UnaryPredicate>
std::pair<FormVector, FormVector> SplitFormsBy(FormVector forms,
                                               UnaryPredicate p) {
  auto first_false = std::partition(forms.begin(), forms.end(), p);

  return std::make_pair(FormVector(std::make_move_iterator(forms.begin()),
                                   std::make_move_iterator(first_false)),
                        FormVector(std::make_move_iterator(first_false),
                                   std::make_move_iterator(forms.end())));
}

// Returns the fields from PasswordForm necessary to match an HTTPS form with
// an invalid signon_realm with the HTTP form credentials from which HTTPS form
// was created.
FormKeyForHttpMatch GetFormKeyForHttpMatch(const autofill::PasswordForm& form) {
  return {form.date_created, form.origin.GetContent(), form.username_value};
}

// Returns the fields from PasswordForm necessary to match an HTTPS form with an
// invalid signon_realm with an HTTPS form with a valid signon_realm. This
// function is used only for matching an HTML credential with an invalid
// signon_realm with an HTML credentils with a valid signon_realm.
FormKeyForHttpsMatch GetFormKeyForHttpsMatch(
    const autofill::PasswordForm& form) {
  DCHECK_EQ(form.scheme, autofill::PasswordForm::Scheme::SCHEME_HTML);
  return {form.origin.GetOrigin().spec(), form.username_value};
}

// Removes HTML forms with HTTPS protocol which have an invalid signon_realm
// from the password store as follow:
// (1) Credentials with the signon_realm equal to the web origin are ignored.
// (2) Blacklisted credentials with an invalid signom_realm are removed.
// (3) Credentials for which an equivalent HTTP credential exists in the
// password store are removed, relying on migrator to recreate them.
// (4) Credentials for which an equivalent HTTPS credential exists in the
// password store are removed as well, because they are already correctly
// recreated.
// (5) If none of these steps happened then the credentials are not
// fixed and they could not be re-created from HTTP credentials. Thus, this
// function will fix their signon_realm.
// Note that credentials with an invalid signon_realm cannot be fixed without
// checking steps (3) and (4) because it might lead to overwriting the most
// up-to-date password value for that site.
void RemoveHtmlCredentialsWithInvalidRealm(
    PasswordStore* store,
    const std::map<FormKeyForHttpMatch, std::string>& http_credentials_map,
    const std::set<FormKeyForHttpsMatch>& https_credentials_keys,
    FormVector https_html_forms) {
  DCHECK(std::all_of(
      https_html_forms.begin(), https_html_forms.end(), [](const auto& form) {
        return form->scheme == autofill::PasswordForm::Scheme::SCHEME_HTML;
      }));

  // Ignore credentials which are valid.
  base::EraseIf(https_html_forms, [](const auto& form) {
    return form->signon_realm == form->origin.GetOrigin().spec();
  });

  for (const auto& form : https_html_forms) {
    store->RemoveLogin(*form);

    if (form->blacklisted_by_user ||
        base::ContainsKey(http_credentials_map,
                          GetFormKeyForHttpMatch(*form)) ||
        base::ContainsKey(https_credentials_keys,
                          GetFormKeyForHttpsMatch(*form))) {
      // If form is blacklisted then it was useless so far.
      // If there is an HTTP match then credentials can be recovered.
      // If there is an HTTPS match then credentials are already recovered.
      // In all cases credentials can be safely removed.
      continue;
    }
    // Fix the signon_realm.
    form->signon_realm = form->origin.GetOrigin().spec();
    store->AddLogin(*form);
  }
}

// Removes non-HTML forms with HTTPS protocol which have an invalid signon_realm
// from the password store as follow:
// (1) Search for an HTTP form with the same date of creation, same origin and
// same username.
// (2) If such a corresponding HTTP credential exists then compare their
// signon_realm (excluding protocol). If the signon_realms are different,
// HTTPS credentials will be deleted from the password store.
void RemoveNonHtmlCredentialsWithInvalidRealm(
    PasswordStore* store,
    const std::map<FormKeyForHttpMatch, std::string>& http_credentials_map,
    FormVector https_non_html_forms) {
  DCHECK(std::all_of(https_non_html_forms.begin(), https_non_html_forms.end(),
                     [](const auto& form) {
                       return form->scheme !=
                              autofill::PasswordForm::Scheme::SCHEME_HTML;
                     }));
  // Credentials with signon_realm different from the origin were not created by
  // faulty migration and are ignored.
  base::EraseIf(https_non_html_forms, [](const auto& form) {
    return form->origin.spec() != form->signon_realm;
  });

  for (const auto& form : https_non_html_forms) {
    // Match HTTPS credentials with HTTP credentials if they have same
    // date_created, origin (excluding protocol) and username_value.
    // Because only forms with signon_realm equal to the origin URL remained at
    // this point (see the erasing above), GURL(form->signon_realm) is the same
    // as form->origin.
    auto it = http_credentials_map.find(GetFormKeyForHttpMatch(*form));
    if (it != http_credentials_map.end() &&
        it->second != form->origin.GetContent()) {
      // The password store contains a corresponding HTTP credential, current
      // HTTPS credential being the migrated version of the HTTP credential.
      // Thus, they must have the same signon_realm (excluding their
      // protocol). Because the current signon_realm (excluding protocol) of
      // the HTTPS credential is different from the signon_realm (excluding
      // protocol) given by the corresponding HTTP credential (which is the
      // good one) means that the HTTPS credential was faulty migrated and
      // thus the form can be removed.
      store->RemoveLogin(*form);
    }
  }
}

}  // namespace

InvalidRealmCredentialCleaner::InvalidRealmCredentialCleaner(
    scoped_refptr<PasswordStore> store,
    PrefService* prefs)
    : store_(std::move(store)), prefs_(prefs) {}

InvalidRealmCredentialCleaner::~InvalidRealmCredentialCleaner() = default;

void InvalidRealmCredentialCleaner::StartCleaning(Observer* observer) {
  DCHECK(observer);
  DCHECK(!observer_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_ = observer;
  remaining_cleaning_tasks_ = 2;
  store_->GetBlacklistLogins(this);
  store_->GetAutofillableLogins(this);
}

void InvalidRealmCredentialCleaner::OnGetPasswordStoreResults(
    FormVector results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Non HTTP or HTTPS credentials are ignored.
  base::EraseIf(results, [](const auto& form) {
    return !form->origin.SchemeIsHTTPOrHTTPS();
  });

  // Separate HTTP and HTTPS credentials.
  FormVector http_forms, https_forms;
  std::tie(http_forms, https_forms) = SplitFormsBy(
      std::move(results),
      [](const auto& form) { return form->origin.SchemeIs(url::kHttpScheme); });

  // Map from (date_created, origin (excluding protocol), username_value) of
  // HTTP forms to the expected signon_realm (excluding the protocol).
  std::map<FormKeyForHttpMatch, std::string> http_credentials_map;
  for (const auto& form : http_forms) {
    http_credentials_map.emplace(
        GetFormKeyForHttpMatch(*form),
        password_manager_util::GetSignonRealmWithProtocolExcluded(*form));
  }

  // Separate HTML and non-HTML HTTPS credentials.
  FormVector https_html_forms, https_non_html_forms;
  std::tie(https_html_forms, https_non_html_forms) =
      SplitFormsBy(std::move(https_forms), [](const auto& form) {
        return form->scheme == autofill::PasswordForm::Scheme::SCHEME_HTML;
      });

  // This set indicates if the password store contains an HTML form with HTTPS
  // protocol with a specific web origin and username. Only forms with the
  // correct signon_realm are inserted in this set.
  std::set<FormKeyForHttpsMatch> https_credentials_keys;
  for (const auto& form : https_html_forms) {
    if (form->signon_realm == form->origin.GetOrigin().spec())
      https_credentials_keys.insert(GetFormKeyForHttpsMatch(*form));
  }

  RemoveHtmlCredentialsWithInvalidRealm(store_.get(), http_credentials_map,
                                        https_credentials_keys,
                                        std::move(https_html_forms));

  RemoveNonHtmlCredentialsWithInvalidRealm(store_.get(), http_credentials_map,
                                           std::move(https_non_html_forms));
  if (--remaining_cleaning_tasks_ == 0)
    CleaningFinished();
}

void InvalidRealmCredentialCleaner::CleaningFinished() {
  // Set the preference in order to avoid calling clean-up again.
  prefs_->SetBoolean(prefs::kCredentialsWithWrongSignonRealmRemoved, true);

  observer_->CleaningCompleted();
}

}  // namespace password_manager