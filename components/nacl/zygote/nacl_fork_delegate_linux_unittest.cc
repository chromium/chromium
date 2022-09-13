// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/zygote/nacl_fork_delegate_linux.h"

#include <memory>

#include "base/environment.h"
#include "base/process/launch.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nacl {

TEST(NaClForkDelegateLinuxTest, EnvPassthrough) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  const char passthrough1[] = "HELPER_PASSTHROUGH1";
  const char passthrough2[] = "HELPER_PASSTHROUGH2";
  const char passthrough3[] = "HELPER_PASSTHROUGH3";
  const char passthrough4[] = "HELPER_PASSTHROUGH4";
  const char passthrough5[] = "NACL_EXE_STDOUT";
  const char value1[] = "passthrough_value1";
  const char value3[] = "passthrough_value3";
  const char value4[] = "passthrough_value4";
  const char value5[] = "passthrough_value5";
  std::string passthrough_value;
  passthrough_value += passthrough1;
  passthrough_value += ",";
  passthrough_value += passthrough2;
  passthrough_value += ",";
  passthrough_value += passthrough3;
  // Not adding passthrough4 to the passthrough variable.
  // Not adding passthrough5 either because it is implicitly allowed.
  env->SetVar("NACL_ENV_PASSTHROUGH", passthrough_value.c_str());
  env->SetVar(passthrough1, value1);
  // Intentionally skip setting a value for passthrough2.
  env->SetVar(passthrough3, value3);
  env->SetVar(passthrough4, value4);
  env->SetVar(passthrough5, value5);

  base::LaunchOptions options;
  NaClForkDelegate::AddPassthroughEnvToOptions(&options);
  EXPECT_EQ(value1, options.environment[passthrough1]);
  EXPECT_EQ(0U, options.environment.count(passthrough2));
  EXPECT_EQ(value3, options.environment[passthrough3]);
  EXPECT_EQ(0U, options.environment.count(passthrough4));
  EXPECT_EQ(value5, options.environment[passthrough5]);
}

}  // namespace nacl
