// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags/flags_ui_handler.h"

#include "base/test/task_environment.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestFlagStorage : public flags_ui::FlagsStorage {
 public:
  // Retrieves the flags as a set of strings.
  std::set<std::string> GetFlags() const override { return flags_; }
  // Stores the |flags| and returns true on success.
  bool SetFlags(const std::set<std::string>& flags) override {
    flags_ = flags;
    return true;
  }

  // Retrieves the serialized origin list corresponding to
  // |internal_entry_name|. Does not check if the return value is well formed.
  std::string GetOriginListFlag(
      const std::string& internal_entry_name) const override {
    if (origin_list_flags_.count(internal_entry_name) == 0) {
      return "";
    }
    return origin_list_flags_.at(internal_entry_name);
  };
  // Sets the serialized |origin_list_value| corresponding to
  // |internal_entry_name|. Does not check if |origin_list_value| is well
  // formed.
  void SetOriginListFlag(const std::string& internal_entry_name,
                         const std::string& origin_list_value) override {
    origin_list_flags_[internal_entry_name] = origin_list_value;
  };

  std::string GetStringFlag(
      const std::string& internal_entry_name) const override {
    return GetOriginListFlag(internal_entry_name);
  }
  void SetStringFlag(const std::string& internal_entry_name,
                     const std::string& value) override {
    SetOriginListFlag(internal_entry_name, value);
  }

  // Lands pending changes to disk immediately.
  void CommitPendingWrites() override{};

 private:
  std::set<std::string> flags_;
  std::map<std::string, std::string> origin_list_flags_;
};

class TestFlagsUIHandler : public FlagsUIHandler {
 public:
  // Make public for testing.
  using FlagsUIHandler::set_web_ui;
};

class FlagsUIHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<TestFlagStorage> storage =
        std::make_unique<TestFlagStorage>();
    storage_ = storage.get();

    handler_ = std::make_unique<TestFlagsUIHandler>();
    handler_->Init(std::move(storage),
                   flags_ui::FlagAccess::kOwnerAccessToFlags);
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
  }

 protected:
  content::TestWebUI web_ui_;
  std::unique_ptr<TestFlagsUIHandler> handler_;
  raw_ptr<TestFlagStorage> storage_;
};

TEST_F(FlagsUIHandlerTest, HandlesSetString) {
  // Need to use an actual feature name for ChromeOS.
  const std::string kTestFeature = "protected-audience-debug-token";
  EXPECT_EQ("", storage_->GetStringFlag(kTestFeature));

  web_ui_.HandleReceivedMessage(
      flags_ui::kSetStringFlag,
      base::Value::List().Append(kTestFeature).Append("value"));
  EXPECT_EQ("value", storage_->GetStringFlag(kTestFeature));
}

TEST_F(FlagsUIHandlerTest, HandlesSetOriginList) {
  // Need to use an actual feature name for ChromeOS.
  const std::string kTestFeature = "isolate-origins";
  EXPECT_EQ("", storage_->GetOriginListFlag(kTestFeature));

  web_ui_.HandleReceivedMessage(
      flags_ui::kSetOriginListFlag,
      base::Value::List()
          .Append(kTestFeature)
          .Append("https://foo.com,invalid,http://bar.org"));
  EXPECT_EQ("https://foo.com,http://bar.org",
            storage_->GetOriginListFlag(kTestFeature));
}

}  // namespace
