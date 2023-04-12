// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/auth_factor_conversions.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cryptohome {
namespace {

class AuthFactorConversionsTest : public testing::Test {
 protected:
  AuthFactorConversionsTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Makes sure that `SafeConvertFactorTypeFromProto` and
// `ConvertFactorTypeFromProto` return corresponding values.
TEST_F(AuthFactorConversionsTest, FactorTypeProtoToChrome) {
  for (user_data_auth::AuthFactorType type = user_data_auth::AuthFactorType_MIN;
       type <= user_data_auth::AuthFactorType_MAX;
       type = static_cast<user_data_auth::AuthFactorType>(type + 1)) {
    absl::optional<AuthFactorType> result =
        SafeConvertFactorTypeFromProto(type);
    SCOPED_TRACE("For user_data_auth::AuthFactorType " +
                 base::NumberToString(type));
    if (result.has_value()) {
      EXPECT_EQ(result.value(), ConvertFactorTypeFromProto(type));
    } else {
      EXPECT_DEATH(ConvertFactorTypeFromProto(type), "FATAL");
    }
  }
}

}  // namespace
}  // namespace cryptohome
