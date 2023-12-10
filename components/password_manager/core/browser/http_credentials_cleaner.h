// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_CREDENTIALS_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_CREDENTIALS_CLEANER_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

class PrefService;

namespace password_manager {

class PasswordStoreInterface;

// This class is responsible for removing obsolete HTTP credentials that can
// safely be deleted and reporting metrics about HTTP to HTTPS migration. This
// class will delete HTTP credentials with HSTS enabled for that site and for
// which an equivalent (i.e. same signon_realm excluding protocol,
// PasswordForm::scheme (i.e. HTML, BASIC, etc.), username and password) HTTPS
// credential exists in the password store. Also it replace HTTP credentials for
// which no HTTPS credential with same signon_realm excluding protocol,
// PasswordForm::scheme and username exists and site has
// HSTS enabled, by an HTTPS version of that form.
class HttpCredentialCleaner : public PasswordStoreConsumer,
                              public CredentialsCleaner {
 public:
  enum class HttpCredentialType {
    kHasConflictingHttpsWithoutHsts = 0,
    kHasConflictingHttpsWithHsts = 1,
    kHasEquivalentHttpsWithoutHsts = 2,
    kHasEquivalentHttpsWithHsts = 3,
    kHasNoMatchingHttpsWithoutHsts = 4,
    kHasNoMatchingHttpsWithHsts = 5,
    kMaxValue = kHasNoMatchingHttpsWithHsts
  };

  // The cleaning will be made for credentials from |store|.
  // |network_context_getter| should return nullptr if it can't get the network
  // context because whatever owns it is dead.
  // A preference from |prefs| is used to set the last time (in seconds) when
  // the cleaning was performed.
  HttpCredentialCleaner(
      scoped_refptr<PasswordStoreInterface> store,
      base::RepeatingCallback<network::mojom::NetworkContext*()>
          network_context_getter,
      PrefService* prefs);

  HttpCredentialCleaner(const HttpCredentialCleaner&) = delete;
  HttpCredentialCleaner& operator=(const HttpCredentialCleaner&) = delete;

  ~HttpCredentialCleaner() override;

  // CredentialsCleaner:
  bool NeedsCleaning() override;
  void StartCleaning(Observer* observer) override;

  // The time that should pass in order to do the clean-up again.
  static constexpr int kCleanUpDelayInDays = 90;

 private:
  // This type define a subset of PasswordForm where first argument is the
  // signon-realm excluding the protocol, the second argument is
  // the PasswordForm::scheme (i.e. HTML, BASIC, etc.) and the third argument is
  // the username of the form.
  using FormKey = std::tuple<std::string, PasswordForm::Scheme, std::u16string>;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // This function will inform us using |hsts_result| parameter if the |form|'s
  // host has HSTS enabled. |key| is |form|'s encoding which is used for
  // matching |form| with an HTTPS credential with the same FormKey.
  // Inside the function the metric counters are updated and, if needed, the
  // |form| is removed or migrated to HTTPS.
  void OnHSTSQueryResult(std::unique_ptr<PasswordForm> form,
                         FormKey key,
                         HSTSResult hsts_result);

  // After all HTTP credentials are processed, this function will inform the
  // |observer_| about completion.
  void SetPrefIfDone();

  // Clean-up is performed on |store_|.
  scoped_refptr<PasswordStoreInterface> store_;

  // Needed to create HSTS request.
  base::RepeatingCallback<network::mojom::NetworkContext*()>
      network_context_getter_;

  // |prefs_| is not an owning pointer. It is used to record he last time (in
  // seconds) when the cleaning was performed.
  raw_ptr<PrefService> prefs_;

  // Map from (signon-realm excluding the protocol, Password::Scheme, username)
  // tuples of HTTPS forms to a list of passwords for that pair.
  std::map<FormKey, base::flat_set<std::u16string>> https_credentials_map_;

  // Used to signal completion of the clean-up. It is null until
  // StartCleaning is called.
  raw_ptr<Observer> observer_ = nullptr;

  // The number of HTTP credentials processed after HSTS query results are
  // received.
  size_t processed_results_ = 0;

  // Number of HTTP credentials from the password store. Used to know when all
  // credentials were processed.
  size_t total_http_credentials_ = 0;

  base::WeakPtrFactory<HttpCredentialCleaner> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_CREDENTIALS_CLEANER_H_
