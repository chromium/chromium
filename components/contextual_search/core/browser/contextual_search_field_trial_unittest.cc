// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/contextual_search_field_trial.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

// Tests ContextualSearchFieldTrial class.
class ContextualSearchFieldTrialTest : public testing::Test {
 public:
  ContextualSearchFieldTrialTest() = default;

  ContextualSearchFieldTrialTest(const ContextualSearchFieldTrialTest&) =
      delete;
  ContextualSearchFieldTrialTest& operator=(
      const ContextualSearchFieldTrialTest&) = delete;

  ~ContextualSearchFieldTrialTest() override {}

  // Inner class that stubs out access to Variations and command-line switches.
  class ContextualSearchFieldTrialStubbed : public ContextualSearchFieldTrial {
   public:
    // Use these to set a non-empty value to override return of a Get method.
    void SetSwitchValue(const std::string& value);
    void SetParamValue(const std::string& value);

   protected:
    // These are overridden to return the Set value above.
    bool HasSwitch(const std::string& name) override;
    std::string GetSwitch(const std::string& name) override;
    std::string GetParam(const std::string& name) override;

   private:
    bool does_have_switch_;
    std::string switch_value_;
    std::string param_value_;
  };

  // The class under test.
  std::unique_ptr<ContextualSearchFieldTrialStubbed> field_trial_;

 protected:
  void SetUp() override {
    field_trial_ = std::make_unique<ContextualSearchFieldTrialStubbed>();
  }

  void TearDown() override {}
};

bool ContextualSearchFieldTrialTest::ContextualSearchFieldTrialStubbed::
    HasSwitch(const std::string& name) {
  return does_have_switch_;
}

void ContextualSearchFieldTrialTest::ContextualSearchFieldTrialStubbed::
    SetSwitchValue(const std::string& value) {
  switch_value_ = value;
  does_have_switch_ = true;
}

std::string
ContextualSearchFieldTrialTest::ContextualSearchFieldTrialStubbed::GetSwitch(
    const std::string& name) {
  return switch_value_;
}

void ContextualSearchFieldTrialTest::ContextualSearchFieldTrialStubbed::
    SetParamValue(const std::string& value) {
  param_value_ = value;
}

std::string
ContextualSearchFieldTrialTest::ContextualSearchFieldTrialStubbed::GetParam(
    const std::string& name) {
  return param_value_;
}

TEST_F(ContextualSearchFieldTrialTest, IntegerDefaultValue) {
  // Should return this default value.
  EXPECT_EQ(
      ContextualSearchFieldTrial::kContextualSearchDefaultSampleSurroundingSize,
      field_trial_->GetSampleSurroundingSize());
}

TEST_F(ContextualSearchFieldTrialTest, IntegerParamOverrides) {
  // Params override defaults.
  field_trial_->SetParamValue("500");
  EXPECT_EQ(500, field_trial_->GetSampleSurroundingSize());
}

TEST_F(ContextualSearchFieldTrialTest, IntegerSwitchOverrides) {
  field_trial_->SetParamValue("500");
  // Switches override params.
  field_trial_->SetSwitchValue("200");
  EXPECT_EQ(200, field_trial_->GetSampleSurroundingSize());
}

TEST_F(ContextualSearchFieldTrialTest, IntegerJunkIgnored) {
  // A junk value effectively resets the switch.
  field_trial_->SetSwitchValue("foo");
  EXPECT_EQ(
      ContextualSearchFieldTrial::kContextualSearchDefaultSampleSurroundingSize,
      field_trial_->GetSampleSurroundingSize());
}

TEST_F(ContextualSearchFieldTrialTest, StringDefaultEmpty) {
  // Default should return an empty string.
  EXPECT_TRUE(field_trial_->GetResolverURLPrefix().empty());
}

TEST_F(ContextualSearchFieldTrialTest, StringParamOverrides) {
  // Params override.
  field_trial_->SetParamValue("param");
  EXPECT_EQ("param", field_trial_->GetResolverURLPrefix());
}

TEST_F(ContextualSearchFieldTrialTest, StringSwitchOverrides) {
  field_trial_->SetParamValue("param");
  // Switches override params.
  field_trial_->SetSwitchValue("switch");
  EXPECT_EQ("switch", field_trial_->GetResolverURLPrefix());
}
