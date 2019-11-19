// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/remote_database_manager.h"

#include <map>
#include <memory>

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/safe_browsing/android/safe_browsing_api_handler.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

class TestSafeBrowsingApiHandler : public SafeBrowsingApiHandler {
 public:
  std::string GetSafetyNetId() override { return ""; }
  void StartURLCheck(std::unique_ptr<URLCheckCallbackMeta> callback,
                     const GURL& url,
                     const SBThreatTypeSet& threat_types) override {}
  bool StartCSDAllowlistCheck(const GURL& url) override { return false; }
  bool StartHighConfidenceAllowlistCheck(const GURL& url) override {
    return false;
  }
};

}  // namespace

class RemoteDatabaseManagerTest : public testing::Test {
 protected:
  RemoteDatabaseManagerTest() {}

  void SetUp() override {
    SafeBrowsingApiHandler::SetInstance(&api_handler_);
    db_ = new RemoteSafeBrowsingDatabaseManager();
  }

  void TearDown() override {
    db_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  // Setup the two field trial params.  These are read in db_'s ctor.
  void SetFieldTrialParams(const std::string types_to_check_val) {
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();

    const std::string group_name = "GroupFoo";  // Value not used
    const std::string experiment_name = "SafeBrowsingAndroid";
    ASSERT_TRUE(
        base::FieldTrialList::CreateFieldTrial(experiment_name, group_name));

    std::map<std::string, std::string> params;
    if (!types_to_check_val.empty())
      params["types_to_check"] = types_to_check_val;

    ASSERT_TRUE(variations::AssociateVariationParams(experiment_name,
                                                     group_name, params));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestSafeBrowsingApiHandler api_handler_;
  scoped_refptr<RemoteSafeBrowsingDatabaseManager> db_;
};

TEST_F(RemoteDatabaseManagerTest, DisabledViaNull) {
  EXPECT_TRUE(db_->IsSupported());

  SafeBrowsingApiHandler::SetInstance(nullptr);
  EXPECT_FALSE(db_->IsSupported());
}

TEST_F(RemoteDatabaseManagerTest, TypesToCheckDefault) {
  // Most are true, a few are false.
  for (int t_int = 0;
       t_int <= static_cast<int>(content::ResourceType::kMaxValue); t_int++) {
    content::ResourceType t = static_cast<content::ResourceType>(t_int);
    switch (t) {
      case content::ResourceType::kStylesheet:
      case content::ResourceType::kImage:
      case content::ResourceType::kFontResource:
      case content::ResourceType::kFavicon:
        EXPECT_FALSE(db_->CanCheckResourceType(t));
        break;
      default:
        EXPECT_TRUE(db_->CanCheckResourceType(t));
        break;
    }
  }
}

TEST_F(RemoteDatabaseManagerTest, TypesToCheckFromTrial) {
  SetFieldTrialParams("1,2,blah, 9");
  db_ = new RemoteSafeBrowsingDatabaseManager();
  EXPECT_TRUE(db_->CanCheckResourceType(
      content::ResourceType::kMainFrame));  // defaulted
  EXPECT_TRUE(db_->CanCheckResourceType(content::ResourceType::kSubFrame));
  EXPECT_TRUE(db_->CanCheckResourceType(content::ResourceType::kStylesheet));
  EXPECT_FALSE(db_->CanCheckResourceType(content::ResourceType::kScript));
  EXPECT_FALSE(db_->CanCheckResourceType(content::ResourceType::kImage));
  // ...
  EXPECT_FALSE(db_->CanCheckResourceType(content::ResourceType::kMedia));
  EXPECT_TRUE(db_->CanCheckResourceType(content::ResourceType::kWorker));
}

}  // namespace safe_browsing
