// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_suggestions_service.h"

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

variations::VariationID kVariationID = 123;

void OnDocumentSuggestionsLoaderAvailable(
    std::unique_ptr<network::SimpleURLLoader> loader) {}

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
    // Set up identity manager.
    identity_test_env_.SetPrimaryAccount("foo@gmail.com");
    identity_test_env_.SetRefreshTokenForPrimaryAccount();
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);

    // Set up a variation.
    variations::VariationsIdsProvider::GetInstance()->ResetForTesting();
    variations::AssociateGoogleVariationID(
        variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, "trial name",
        "group name", kVariationID);
    base::FieldTrialList::CreateFieldTrial("trial name", "group name")->group();
  }
  DocumentSuggestionsServiceTest(const DocumentSuggestionsServiceTest&) =
      delete;
  DocumentSuggestionsServiceTest& operator=(
      const DocumentSuggestionsServiceTest&) = delete;

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<DocumentSuggestionsService> document_suggestions_service_;
};

TEST_F(DocumentSuggestionsServiceTest, VariationHeaders) {
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([](const network::ResourceRequest& request) {
        EXPECT_TRUE(variations::HasVariationsHeader(request));
        std::string variation = variations::VariationsIdsProvider::GetInstance()
                                    ->GetVariationsString();
        EXPECT_EQ(variation, " " + base::NumberToString(kVariationID) + " ");
      }));

  document_suggestions_service_->CreateDocumentSuggestionsRequest(
      base::ASCIIToUTF16(""), false,
      base::BindOnce(OnDocumentSuggestionsLoaderAvailable),
      base::BindOnce(OnURLLoadComplete));

  base::RunLoop().RunUntilIdle();
}

}  // namespace
