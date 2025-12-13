// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/rules_service_base.h"

#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {
namespace {

constexpr size_t kFirstRuleIndex = 0;
constexpr char kFirstRuleID[] = "1234";

GURL google_url() {
  return GURL("https://google.com");
}

void ExpectBlockVerdict(Verdict verdict, bool expect_machine_source = true) {
  ASSERT_EQ(verdict.level(), Rule::Level::kBlock);
  EXPECT_EQ(verdict.triggered_rules().size(), 1u);

  auto index = Verdict::TriggeredRuleKey{
      .index = kFirstRuleIndex,
      .machine_scope = expect_machine_source,
  };
  EXPECT_TRUE(verdict.triggered_rules().count(index));
  EXPECT_EQ(verdict.triggered_rules().at(index).rule_name, "block");
  EXPECT_EQ(verdict.triggered_rules().at(index).rule_id, kFirstRuleID);
}

void ExpectWarnVerdict(Verdict verdict, bool expect_machine_source = true) {
  ASSERT_EQ(verdict.level(), Rule::Level::kWarn);
  EXPECT_EQ(verdict.triggered_rules().size(), 1u);

  auto index = Verdict::TriggeredRuleKey{
      .index = kFirstRuleIndex,
      .machine_scope = expect_machine_source,
  };
  EXPECT_TRUE(verdict.triggered_rules().count(index));
  EXPECT_EQ(verdict.triggered_rules().at(index).rule_name, "warn");
  EXPECT_EQ(verdict.triggered_rules().at(index).rule_id, kFirstRuleID);
}

void ExpectAllowVerdict(Verdict verdict) {
  ASSERT_EQ(verdict.level(), Rule::Level::kAllow);
  EXPECT_TRUE(verdict.triggered_rules().empty());
}

void ExpectNoVerdict(Verdict verdict) {
  ASSERT_EQ(verdict.level(), Rule::Level::kNotSet);
  EXPECT_TRUE(verdict.triggered_rules().empty());
}

class TestRulesServiceBase : public RulesServiceBase {
 public:
  TestRulesServiceBase(PrefService* prefs, bool incognito)
      : RulesServiceBase(prefs), incognito_(incognito) {}

  bool incognito_profile() const override { return incognito_; }

 private:
  bool incognito_;
};

class RulesServiceBaseTest : public testing::Test {
 public:
  RulesServiceBaseTest() {
    RegisterProfilePrefs(prefs_.registry());
    service_ = std::make_unique<TestRulesServiceBase>(&prefs_, false);
    incognito_service_ = std::make_unique<TestRulesServiceBase>(&prefs_, true);
  }

  void TearDown() override { SetDataControls(&prefs_, {}); }

 protected:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TestRulesServiceBase> service_;
  std::unique_ptr<TestRulesServiceBase> incognito_service_;
};

}  // namespace

TEST_F(RulesServiceBaseTest, NoRules) {
  ExpectNoVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectNoVerdict(
      incognito_service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectNoVerdict(service_->GetCopyToOSClipboardVerdict(google_url()));
  ExpectNoVerdict(
      incognito_service_->GetCopyToOSClipboardVerdict(google_url()));
}

TEST_F(RulesServiceBaseTest, PolicyScope) {
  constexpr char kPolicy[] = R"({
                    "name": "block",
                    "rule_id": "1234",
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })";

  SetDataControls(&prefs_, {kPolicy}, true);
  ExpectBlockVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()),
                     /*expect_machine_scope=*/true);

  SetDataControls(&prefs_, {kPolicy}, false);
  ExpectBlockVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()),
                     /*expect_machine_scope=*/false);
}

TEST_F(RulesServiceBaseTest, BlockCopyForSourceUrl) {
  SetDataControls(&prefs_, {R"({
                    "name": "block",
                    "rule_id": "1234",
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  ExpectBlockVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectBlockVerdict(
      incognito_service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectBlockVerdict(service_->GetCopyToOSClipboardVerdict(google_url()));
  ExpectBlockVerdict(
      incognito_service_->GetCopyToOSClipboardVerdict(google_url()));
}

TEST_F(RulesServiceBaseTest, WarnCopyForSourceUrl) {
  SetDataControls(&prefs_, {R"({
                    "name": "warn",
                    "rule_id": "1234",
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});

  ExpectWarnVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectWarnVerdict(
      incognito_service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectWarnVerdict(service_->GetCopyToOSClipboardVerdict(google_url()));
  ExpectWarnVerdict(
      incognito_service_->GetCopyToOSClipboardVerdict(google_url()));
}

TEST_F(RulesServiceBaseTest, AllowCopyForSourceUrl) {
  // When multiple rules are triggered, "ALLOW" should have precedence over
  // any other value.
  SetDataControls(&prefs_, {
                               R"({
                    "name": "allow",
                    "rule_id": "1234",
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "ALLOW"}
                    ]
                  })",
                               R"({
                    "name": "warn",
                    "rule_id": "5678",
                    "sources": {
                      "urls": ["https://*"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  ExpectAllowVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectAllowVerdict(
      incognito_service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectAllowVerdict(service_->GetCopyToOSClipboardVerdict(google_url()));
  ExpectAllowVerdict(
      incognito_service_->GetCopyToOSClipboardVerdict(google_url()));
}

TEST_F(RulesServiceBaseTest, BlockCopyForIncognitoSource) {
  SetDataControls(&prefs_, {R"({
                    "name": "block",
                    "rule_id": "1234",
                    "sources": {
                      "incognito": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  ExpectNoVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectBlockVerdict(
      incognito_service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectNoVerdict(service_->GetCopyToOSClipboardVerdict(google_url()));
  ExpectBlockVerdict(
      incognito_service_->GetCopyToOSClipboardVerdict(google_url()));
}

TEST_F(RulesServiceBaseTest, WarnCopyForIncognitoSource) {
  SetDataControls(&prefs_, {R"({
                    "name": "warn",
                    "rule_id": "1234",
                    "sources": {
                      "incognito": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  ExpectNoVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectWarnVerdict(
      incognito_service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectNoVerdict(service_->GetCopyToOSClipboardVerdict(google_url()));
  ExpectWarnVerdict(
      incognito_service_->GetCopyToOSClipboardVerdict(google_url()));
}

TEST_F(RulesServiceBaseTest, BlockOSClipboardCopy) {
  SetDataControls(&prefs_, {R"({
                    "name": "block",
                    "rule_id": "1234",
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  ExpectNoVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectNoVerdict(
      incognito_service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectBlockVerdict(service_->GetCopyToOSClipboardVerdict(google_url()));
  ExpectBlockVerdict(
      incognito_service_->GetCopyToOSClipboardVerdict(google_url()));
}

TEST_F(RulesServiceBaseTest, WarnOSClipboardCopy) {
  SetDataControls(&prefs_, {R"({
                    "name": "warn",
                    "rule_id": "1234",
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "WARN"}
                    ]
                  })"});
  ExpectNoVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectNoVerdict(
      incognito_service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectWarnVerdict(service_->GetCopyToOSClipboardVerdict(google_url()));
  ExpectWarnVerdict(
      incognito_service_->GetCopyToOSClipboardVerdict(google_url()));
}

TEST_F(RulesServiceBaseTest, RuleWithoutRestrictionsForCopy) {
  SetDataControls(&prefs_, {R"({
                    "name": "block",
                    "rule_id": "1234",
                    "destinations": {
                      "urls": ["google.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});
  ExpectNoVerdict(service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectNoVerdict(
      incognito_service_->GetCopyRestrictedBySourceVerdict(google_url()));
  ExpectNoVerdict(service_->GetCopyToOSClipboardVerdict(google_url()));
  ExpectNoVerdict(
      incognito_service_->GetCopyToOSClipboardVerdict(google_url()));
}

}  // namespace data_controls
