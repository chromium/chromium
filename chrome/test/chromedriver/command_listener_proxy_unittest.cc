// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/command_listener_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockCommandListener : public CommandListener {
 public:
  MockCommandListener() : called_(false) {}
  ~MockCommandListener() override {}

  Status BeforeCommand(const std::string& command_name) override {
    called_ = true;
    EXPECT_STREQ("cmd", command_name.c_str());
    return Status(kOk);
  }

  void VerifyCalled() {
    EXPECT_TRUE(called_);
  }

  void VerifyNotCalled() {
    EXPECT_FALSE(called_);
  }

 private:
  bool called_;
};

}  // namespace

TEST(CommandListenerProxy, ForwardsCommands) {
  MockCommandListener listener;
  listener.VerifyNotCalled();
  CommandListenerProxy proxy(&listener);
  listener.VerifyNotCalled();
  ASSERT_EQ(kOk, proxy.BeforeCommand("cmd").code());
  listener.VerifyCalled();
}
