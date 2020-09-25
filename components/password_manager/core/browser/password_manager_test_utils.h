// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_

#include <iosfwd>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
#include "components/password_manager/core/browser/password_hash_data.h"  // nogncheck
#include "components/password_manager/core/browser/password_reuse_detector_consumer.h"  // nogncheck
#endif

namespace password_manager {

// This template allows creating methods with signature conforming to
// TestingFactory of the appropriate platform instance of KeyedServiceFactory.
// Context is the browser context prescribed by TestingFactory. Store is the
// PasswordStore version needed in the tests which use this method.
template <class Context, class Store>
scoped_refptr<RefcountedKeyedService> BuildPasswordStore(Context* context) {
  scoped_refptr<password_manager::PasswordStore> store(new Store);
  if (!store->Init(nullptr))
    return nullptr;
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
  if (!store->Init(nullptr))
    return nullptr;
  return store;
}

// Struct used for creation of PasswordForms from static arrays of data.
// Note: This is only meant to be used in unit test.
struct PasswordFormData {
  const PasswordForm::Scheme scheme;
  const char* signon_realm;
  const char* origin;
  const char* action;
  const wchar_t* submit_element;
  const wchar_t* username_element;
  const wchar_t* password_element;
  const wchar_t* username_value;  // Set to NULL for a blacklist entry.
  const wchar_t* password_value;
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

class MockPasswordStoreObserver : public PasswordStore::Observer {
 public:
  MockPasswordStoreObserver();
  ~MockPasswordStoreObserver() override;

  MOCK_METHOD1(OnLoginsChanged, void(const PasswordStoreChangeList& changes));
};

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
class MockPasswordReuseDetectorConsumer : public PasswordReuseDetectorConsumer {
 public:
  MockPasswordReuseDetectorConsumer();
  ~MockPasswordReuseDetectorConsumer() override;

  MOCK_METHOD5(OnReuseCheckDone,
               void(bool,
                    size_t,
                    base::Optional<PasswordHashData>,
                    const std::vector<MatchingReusedCredential>&,
                    int));
};

// Matcher class used to compare PasswordHashData in tests.
class PasswordHashDataMatcher
    : public ::testing::MatcherInterface<base::Optional<PasswordHashData>> {
 public:
  explicit PasswordHashDataMatcher(base::Optional<PasswordHashData> expected);
  virtual ~PasswordHashDataMatcher() {}

  // ::testing::MatcherInterface overrides
  virtual bool MatchAndExplain(base::Optional<PasswordHashData> hash_data,
                               ::testing::MatchResultListener* listener) const;
  virtual void DescribeTo(::std::ostream* os) const;
  virtual void DescribeNegationTo(::std::ostream* os) const;

 private:
  const base::Optional<PasswordHashData> expected_;

  DISALLOW_COPY_AND_ASSIGN(PasswordHashDataMatcher);
};

::testing::Matcher<base::Optional<PasswordHashData>> Matches(
    base::Optional<PasswordHashData> expected);
#endif

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_TEST_UTILS_H_
