// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/logging/session_log.h"

#include <memory>
#include <optional>
#include <string>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class SessionLogTest : public testing::Test {
 protected:
  void SetUp() override {
    original_value_ = env_->GetVar(kSessionLogDirEnvVar);
  }

  void TearDown() override {
    if (original_value_.has_value()) {
      env_->SetVar(kSessionLogDirEnvVar, *original_value_);
    } else {
      env_->UnSetVar(kSessionLogDirEnvVar);
    }
  }

  std::unique_ptr<base::Environment> env_ = base::Environment::Create();

 private:
  std::optional<std::string> original_value_;
};

TEST_F(SessionLogTest, GetSessionLogDir) {
  ASSERT_TRUE(env_->SetVar(kSessionLogDirEnvVar, "/path/to/session/log"));
  EXPECT_EQ(base::FilePath("/path/to/session/log"), GetSessionLogDir());
}

}  // namespace ash
