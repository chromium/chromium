// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_

#include <iosfwd>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/fake_password_store_backend.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_hash_data.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace password_manager {

// This template allows creating methods with signature conforming to
// TestingFactory of the appropriate platform instance of KeyedServiceFactory.
// Context is the browser context prescribed by TestingFactory. Store is the
// PasswordStore version needed in the tests which use this method.
template <class Context, class Store>
scoped_refptr<RefcountedKeyedService> BuildPasswordStore(Context* context) {
  scoped_refptr<password_manager::PasswordStore> store(new Store);
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  return store;
}

template <class Context, class Store>
scoped_refptr<RefcountedKeyedService> BuildPasswordStoreInterface(
    Context* context) {
  scoped_refptr<password_manager::PasswordStoreInterface> store(new Store);
  return store;
}

// As above, but allows passing parameters to the to-be-created store. The
// parameters are specified *before* context so that they can be bound (as in
// base::BindRepeating(&BuildPasswordStoreWithArgs<...>, my_arg)), leaving
// |context| as a free parameter for TestingFactory.
template <class Context, class Store, typename... Args>
scoped_refptr<RefcountedKeyedService> BuildPasswordStoreWithArgs(
    Args... args,
    Context* context) {
  scoped_refptr<password_manager::PasswordStore> store(
      new Store(std::forward<Args>(args)...));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  return store;
}

// Helper function that builds a real password store with a fake backend.
// Context is the browser context prescribed by TestingFactory.
template <class Context>
scoped_refptr<RefcountedKeyedService> BuildPasswordStoreWithFakeBackend(
    Context* context) {
  return password_manager::BuildPasswordStoreWithArgs<
      Context, password_manager::PasswordStore,
      std::unique_ptr<password_manager::FakePasswordStoreBackend>>(
      std::make_unique<password_manager::FakePasswordStoreBackend>(), context);
}

// Struct used for creation of PasswordForms from static arrays of data.
// Note: This is only meant to be used in unit test.
struct PasswordFormData {
  const PasswordForm::Scheme scheme;
  const char* signon_realm;
  const char* origin;
  const char* action;
  const char16_t* submit_element;
  const char16_t* username_element;
  const char16_t* password_element;
  const char16_t* username_value;  // Set to NULL for a blocklist entry.
  const char16_t* password_value;
  const double last_usage_time;
  const double creation_time;
};

// Creates and returns a new PasswordForm built from |form_data|.
std::unique_ptr<PasswordForm> PasswordFormFromData(
    const PasswordFormData& form_data);

// Like PasswordFormFromData(), but also fills arbitrary values into fields not
// specified by |form_data|.  This may be useful e.g. for tests looking to
// verify the handling of these fields.  If |use_federated_login| is true, this
// function will set the form's |federation_origin|.
std::unique_ptr<PasswordForm> FillPasswordFormWithData(
    const PasswordFormData& form_data,
    bool use_federated_login = false);

// Creates a new vector entry. Callers are expected to call .get() to get a raw
// pointer to the underlying PasswordForm.
std::unique_ptr<PasswordForm> CreateEntry(const std::string& username,
                                          const std::string& password,
                                          const GURL& origin_url,
                                          bool is_psl_match,
                                          bool is_affiliation_based_match);

// Checks whether the PasswordForms pointed to in |actual_values| are in some
// permutation pairwise equal to those in |expectations|. Returns true in case
// of a perfect match; otherwise returns false and writes details of mismatches
// in human readable format to |mismatch_output| unless it is null.
// Note: |expectations| should be a const ref, but needs to be a const pointer,
// because GMock tried to copy the reference by value.
bool ContainsEqualPasswordFormsUnordered(
    const std::vector<std::unique_ptr<PasswordForm>>& expectations,
    const std::vector<std::unique_ptr<PasswordForm>>& actual_values,
    std::ostream* mismatch_output);

MATCHER_P(UnorderedPasswordFormElementsAre, expectations, "") {
  return ContainsEqualPasswordFormsUnordered(*expectations, arg,
                                             result_listener->stream());
}

MATCHER_P(LoginsResultsOrErrorAre, expectations, "") {
  if (absl::holds_alternative<PasswordStoreBackendError>(arg))
    return false;

  return ContainsEqualPasswordFormsUnordered(
      *expectations, std::move(absl::get<LoginsResult>(arg)),
      result_listener->stream());
}

class MockPasswordStoreObserver : public PasswordStoreInterface::Observer {
 public:
  MockPasswordStoreObserver();
  ~MockPasswordStoreObserver() override;

  MOCK_METHOD((void),
              OnLoginsChanged,
              (PasswordStoreInterface * store,
               const PasswordStoreChangeList& changes),
              (override));
  MOCK_METHOD((void),
              OnLoginsRetained,
              (PasswordStoreInterface * store,
               const std::vector<PasswordForm>& retained_passwords),
              (override));
};

class MockPasswordReuseDetectorConsumer : public PasswordReuseDetectorConsumer {
 public:
  MockPasswordReuseDetectorConsumer();
  ~MockPasswordReuseDetectorConsumer() override;

  MOCK_METHOD((void),
              OnReuseCheckDone,
              (bool,
               size_t,
               absl::optional<PasswordHashData>,
               const std::vector<MatchingReusedCredential>&,
               int,
               const std::string&,
               uint64_t),
              (override));
};

// Matcher class used to compare PasswordHashData in tests.
class PasswordHashDataMatcher
    : public ::testing::MatcherInterface<absl::optional<PasswordHashData>> {
 public:
  explicit PasswordHashDataMatcher(absl::optional<PasswordHashData> expected);

  PasswordHashDataMatcher(const PasswordHashDataMatcher&) = delete;
  PasswordHashDataMatcher& operator=(const PasswordHashDataMatcher&) = delete;

  ~PasswordHashDataMatcher() override;

  // ::testing::MatcherInterface overrides
  bool MatchAndExplain(absl::optional<PasswordHashData> hash_data,
                       ::testing::MatchResultListener* listener) const override;
  void DescribeTo(::std::ostream* os) const override;
  void DescribeNegationTo(::std::ostream* os) const override;

 private:
  const absl::optional<PasswordHashData> expected_;
};

::testing::Matcher<absl::optional<PasswordHashData>> Matches(
    absl::optional<PasswordHashData> expected);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
