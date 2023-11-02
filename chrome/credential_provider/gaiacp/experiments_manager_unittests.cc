// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/extension/task.h"
#include "chrome/credential_provider/gaiacp/experiments_fetcher.h"
#include "chrome/credential_provider/gaiacp/experiments_manager.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "chrome/credential_provider/test/gls_runner_test_base.h"
#include "chrome/credential_provider/test/test_credential.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class ExperimentsManagerTest : public GlsRunnerTestBase {
 protected:
  void SetUp() override;
};

void ExperimentsManagerTest::SetUp() {
  GlsRunnerTestBase::SetUp();

  ASSERT_EQ(S_OK, SetGlobalFlagForTesting(L"experiments_enabled", 1));
  FakesForTesting fakes;
  fakes.fake_win_http_url_fetcher_creator =
      fake_http_url_fetcher_factory()->GetCreatorCallback();
  WinHttpUrlFetcher::SetCreatorForTesting(
      fakes.fake_win_http_url_fetcher_creator);

  // Set token result a valid access token.
  fake_http_url_fetcher_factory()->SetFakeResponse(
      GURL(GaiaUrls::GetInstance()->oauth2_token_url().spec().c_str()),
      FakeWinHttpUrlFetcher::Headers(), "{\"access_token\": \"dummy_token\"}");
}

TEST_F(ExperimentsManagerTest, DefaultValues) {
  EXPECT_EQ("false", ExperimentsManager::Get()->GetExperimentForUser(
                         "test_sid", Experiment::TEST_CLIENT_FLAG));
  EXPECT_EQ("false", ExperimentsManager::Get()->GetExperimentForUser(
                         "test_sid", Experiment::TEST_CLIENT_FLAG2));

  EXPECT_FALSE(ExperimentsManager::Get()->GetExperimentForUserAsBool(
      "test_sid", Experiment::TEST_CLIENT_FLAG));
  EXPECT_FALSE(ExperimentsManager::Get()->GetExperimentForUserAsBool(
      "test_sid", Experiment::TEST_CLIENT_FLAG2));
}

// Tests different outcomes of fetching experiments through GCPW:
// 0 - Experiments are fetched successfully.
// 1 - Failed fetching experiments.
// 2 - Response in fetching experiments contains malformed payload.
class ExperimentsManagerGcpwE2ETest
    : public ExperimentsManagerTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(ExperimentsManagerGcpwE2ETest, FetchingExperiments) {
  int experiment_fetch_status = GetParam();

  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", L"password", L"Full Name", L"comment",
                base::UTF8ToWide(kDefaultGaiaId), L"user@company.com", &sid));

  std::wstring device_resource_id = L"foo_resource_id";
  ASSERT_EQ(S_OK, SetUserProperty(OLE2W(sid), L"device_resource_id",
                                  device_resource_id));

  // Re-registering effectively clears the experiment values from the previous
  // fetches.
  ExperimentsManager::Get()->RegisterExperiments();

  GURL url = ExperimentsFetcher::Get()->GetExperimentsUrl();

  if (experiment_fetch_status == 0) {
    fake_http_url_fetcher_factory()->SetFakeResponse(
        url, FakeWinHttpUrlFetcher::Headers(),
        "{\"experiments\": [{\"feature\": \"test_client_flag\", \"value\": "
        "\"abc\"}, {\"feature\": \"test_client_flag2\", \"value\": \"def\"} ] "
        "}");
  } else if (experiment_fetch_status == 1) {
    fake_http_url_fetcher_factory()->SetFakeFailedResponse(url, E_FAIL);
  } else {
    fake_http_url_fetcher_factory()->SetFakeResponse(
        url, FakeWinHttpUrlFetcher::Headers(), "{\"bad_experiments\": [ ] }");
  }

  // Create provider and start logon.
  Microsoft::WRL::ComPtr<ICredentialProviderCredential> cred;

  ASSERT_EQ(S_OK, InitializeProviderAndGetCredential(0, &cred));

  Microsoft::WRL::ComPtr<ITestCredential> test;
  ASSERT_EQ(S_OK, cred.As(&test));

  ASSERT_EQ(S_OK, StartLogonProcessAndWait());

  // Email should be the same as the default one.
  EXPECT_EQ(test->GetFinalEmail(), kDefaultEmail);

  std::string experiment1_value = "false";
  std::string experiment2_value = "false";

  if (experiment_fetch_status == 0) {
    experiment1_value = "abc";
    experiment2_value = "def";
  }

  EXPECT_EQ(experiment1_value,
            ExperimentsManager::Get()->GetExperimentForUser(
                base::WideToUTF8(OLE2W(sid)), Experiment::TEST_CLIENT_FLAG));
  EXPECT_EQ(experiment2_value,
            ExperimentsManager::Get()->GetExperimentForUser(
                base::WideToUTF8(OLE2W(sid)), Experiment::TEST_CLIENT_FLAG2));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExperimentsManagerGcpwE2ETest,
                         ::testing::Values(0, 1, 2));

// Tests different outcomes of fetching experiments through ESA:
// 0 - Experiments are fetched successfully.
// 1 - Failed fetching experiments.
// 2 - Response in fetching experiments contains malformed payload.
class ExperimentsManagerESAE2ETest : public ExperimentsManagerTest,
                                     public ::testing::WithParamInterface<int> {
};

TEST_P(ExperimentsManagerESAE2ETest, FetchingExperiments) {
  int experiment_fetch_status = GetParam();

  // Create a fake user that has the same gaia id as the test gaia id.
  CComBSTR sid;
  ASSERT_EQ(S_OK,
            fake_os_user_manager()->CreateTestOSUser(
                L"foo", L"password", L"Full Name", L"comment",
                base::UTF8ToWide(kDefaultGaiaId), L"user@company.com", &sid));

  ASSERT_EQ(S_OK, GenerateGCPWDmToken((BSTR)sid));

  std::wstring device_resource_id = L"foo_resource_id";
  ASSERT_EQ(S_OK, SetUserProperty(OLE2W(sid), L"device_resource_id",
                                  device_resource_id));

  // Re-registering effectively clears the experiment values from the previous
  // fetches.
  ExperimentsManager::Get()->RegisterExperiments();

  GURL url = ExperimentsFetcher::Get()->GetExperimentsUrl();

  if (experiment_fetch_status == 0) {
    fake_http_url_fetcher_factory()->SetFakeResponse(
        url, FakeWinHttpUrlFetcher::Headers(),
        "{\"experiments\": [{\"feature\": \"test_client_flag\", \"value\": "
        "\"abc\"}, {\"feature\": \"test_client_flag2\", \"value\": \"def\"} ] "
        "}");
  } else if (experiment_fetch_status == 1) {
    fake_http_url_fetcher_factory()->SetFakeFailedResponse(url, E_FAIL);
  } else {
    fake_http_url_fetcher_factory()->SetFakeResponse(
        url, FakeWinHttpUrlFetcher::Headers(), "{\"bad_experiments\": [ ] }");
  }

  std::wstring dm_token;
  ASSERT_EQ(S_OK, GetGCPWDmToken((BSTR)sid, &dm_token));

  std::unique_ptr<extension::Task> task(
      ExperimentsFetcher::GetFetchExperimentsTaskCreator().Run());
  task->SetContext({{device_resource_id, L"", L"", OLE2W(sid), dm_token}});
  task->Execute();

  std::string experiment1_value = "false";
  std::string experiment2_value = "false";

  if (experiment_fetch_status == 0) {
    experiment1_value = "abc";
    experiment2_value = "def";
  }

  EXPECT_EQ(experiment1_value,
            ExperimentsManager::Get()->GetExperimentForUser(
                base::WideToUTF8(OLE2W(sid)), Experiment::TEST_CLIENT_FLAG));
  EXPECT_EQ(experiment2_value,
            ExperimentsManager::Get()->GetExperimentForUser(
                base::WideToUTF8(OLE2W(sid)), Experiment::TEST_CLIENT_FLAG2));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExperimentsManagerESAE2ETest,
                         ::testing::Values(0, 1, 2));

}  // namespace testing
}  // namespace credential_provider
