// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_suggestions_service.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

variations::VariationID kVariationID = 123;

void OnDocumentSuggestionsRequestAvailable(network::ResourceRequest* request) {}

void OnDocumentSuggestionsLoaderAvailable(
    std::unique_ptr<network::SimpleURLLoader> loader,
    const std::string& request_body) {}

void OnURLLoadComplete(const network::SimpleURLLoader* source,
                       std::unique_ptr<std::string> response_body) {}

class DocumentSuggestionsServiceTest : public testing::Test {
 protected:
  DocumentSuggestionsServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        identity_test_env_(&test_url_loader_factory_, &prefs_),
        document_suggestions_service_(new DocumentSuggestionsService(
            identity_test_env_.identity_manager(),
            shared_url_loader_factory_)) {
    // Set up a variation.
    variations::AssociateGoogleVariationID(
        variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, "trial name",
        "group name", kVariationID);
    base::FieldTrialList::CreateFieldTrial("trial name", "group name")
        ->Activate();
  }
  DocumentSuggestionsServiceTest(const DocumentSuggestionsServiceTest&) =
      delete;
  DocumentSuggestionsServiceTest& operator=(
      const DocumentSuggestionsServiceTest&) = delete;

  AccountInfo SetUpPrimaryAccount() {
    auto account_info = identity_test_env_.MakePrimaryAccountAvailable(
        "foo@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetRefreshTokenForPrimaryAccount();
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    return account_info;
  }

  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<DocumentSuggestionsService> document_suggestions_service_;
};

TEST_F(DocumentSuggestionsServiceTest, EnsureVariationHeaders) {
  SetUpPrimaryAccount();

  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  document_suggestions_service_->CreateDocumentSuggestionsRequest(
      u"", false, base::BindOnce(OnDocumentSuggestionsRequestAvailable),
      base::BindOnce(OnDocumentSuggestionsLoaderAvailable),
      base::BindOnce(OnURLLoadComplete));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(variations::HasVariationsHeader(resource_request));
  std::string variation =
      variations::VariationsIdsProvider::GetInstance()->GetVariationsString();
  EXPECT_EQ(variation, " " + base::NumberToString(kVariationID) + " ");
}

TEST_F(DocumentSuggestionsServiceTest, EnsureCookies) {
  SetUpPrimaryAccount();

  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  document_suggestions_service_->CreateDocumentSuggestionsRequest(
      u"", false, base::BindOnce(OnDocumentSuggestionsRequestAvailable),
      base::BindOnce(OnDocumentSuggestionsLoaderAvailable),
      base::BindOnce(OnURLLoadComplete));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(GURL("https://cloudsearch.googleapis.com"))))
      << resource_request.site_for_cookies.ToDebugString();
}

TEST_F(DocumentSuggestionsServiceTest, EnsurePrimaryAccount) {
  EXPECT_FALSE(document_suggestions_service_->HasPrimaryAccount());

  // Make the primary account available.
  SetUpPrimaryAccount();

  EXPECT_TRUE(document_suggestions_service_->HasPrimaryAccount());
}

TEST_F(DocumentSuggestionsServiceTest, EnsureIsSubjectToEnterprisePolicies) {
  EXPECT_EQ(signin::Tribool::kFalse,
            document_suggestions_service_
                ->account_is_subject_to_enterprise_policies());

  // Make the primary account available.
  auto account_info = SetUpPrimaryAccount();

  EXPECT_EQ(signin::Tribool::kUnknown,
            document_suggestions_service_
                ->account_is_subject_to_enterprise_policies());

  // Make the the primary account subject to Enterprise policies.
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_enterprise_policies(true);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  EXPECT_EQ(signin::Tribool::kTrue,
            document_suggestions_service_
                ->account_is_subject_to_enterprise_policies());

  // Make the the primary account NOT subject to Enterprise policies.
  mutator.set_is_subject_to_enterprise_policies(false);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  EXPECT_EQ(signin::Tribool::kFalse,
            document_suggestions_service_
                ->account_is_subject_to_enterprise_policies());
}

}  // namespace
