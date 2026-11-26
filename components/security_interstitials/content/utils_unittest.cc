// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/utils.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace security_interstitials {

namespace {

class FakeEnvironment : public base::Environment {
 public:
  std::optional<std::string> GetVar(base::cstring_view name) override {
    auto it = vars_.find(std::string(name));
    if (it == vars_.end()) {
      return std::nullopt;
    }
    return it->second;
  }
  bool SetVar(base::cstring_view name, const std::string& value) override {
    vars_[std::string(name)] = value;
    return true;
  }
  bool UnSetVar(base::cstring_view name) override {
    vars_.erase(std::string(name));
    return true;
  }

 private:
  std::map<std::string, std::string> vars_;
};

class DateAndTimeSettingsCommandTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Install every known settings tool so that the choice depends only on
    // the desktop environment.
    for (const char* name : {"unity-control-center", "gnome-control-center",
                             "kcmshell4", "kcmshell5", "kcmshell6"}) {
      base::FilePath path = temp_dir_.GetPath().Append(name);
      ASSERT_TRUE(base::WriteFile(path, ""));
      ASSERT_TRUE(base::SetPosixFilePermissions(path, 0700));
    }
    env_.SetVar("PATH", temp_dir_.GetPath().value());
  }

  base::ScopedTempDir temp_dir_;
  FakeEnvironment env_;
};

TEST_F(DateAndTimeSettingsCommandTest, KdePlasma5UsesKcmshell5) {
  env_.SetVar("XDG_CURRENT_DESKTOP", "KDE");
  env_.SetVar("KDE_SESSION_VERSION", "5");
  EXPECT_THAT(GetDateAndTimeSettingsCommand(&env_),
              testing::ElementsAre("kcmshell5", "clock"));
}

TEST_F(DateAndTimeSettingsCommandTest, KdePlasma6UsesKcmshell6) {
  env_.SetVar("XDG_CURRENT_DESKTOP", "KDE");
  env_.SetVar("KDE_SESSION_VERSION", "6");
  EXPECT_THAT(GetDateAndTimeSettingsCommand(&env_),
              testing::ElementsAre("kcmshell6", "kcm_clock"));
}

}  // namespace

}  // namespace security_interstitials
