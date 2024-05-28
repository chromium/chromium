// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_service.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

class EnterpriseCompanionServiceTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment environment_;
};

TEST_F(EnterpriseCompanionServiceTest, Shutdown) {
  base::MockCallback<base::OnceClosure> shutdown_callback;
  base::RunLoop service_run_loop;
  std::unique_ptr<EnterpriseCompanionService> service =
      CreateEnterpriseCompanionService(service_run_loop.QuitClosure());

  EXPECT_CALL(shutdown_callback, Run()).Times(1);
  service->Shutdown(shutdown_callback.Get());
  service_run_loop.Run(FROM_HERE);
}

}  // namespace enterprise_companion
