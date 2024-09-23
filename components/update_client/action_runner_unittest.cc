// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_runner.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/update_client/test_installer.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

namespace {

class FakeActionHandler : public ActionHandler {
 public:
  void Handle(const base::FilePath& action,
              const std::string& session_id,
              Callback callback) override {
    ran_ = true;
    std::move(callback).Run(true, 0, 0);
  }

  bool ran() const { return ran_; }

 protected:
  ~FakeActionHandler() override = default;

 private:
  bool ran_ = false;
};

}  // namespace

TEST(ActionRunnerTest, ErrorOnMissingPath) {
  base::test::TaskEnvironment task_environment;
  bool success = false;
  auto handler = base::MakeRefCounted<FakeActionHandler>();
  base::RunLoop runloop;
  RunAction(handler, base::MakeRefCounted<TestInstaller>(), "file", "sid",
            base::BindLambdaForTesting(
                [&](bool succeeded, int error_code, int extra_code1) {
                  success = succeeded;
                  runloop.Quit();
                }));
  runloop.Run();
  ASSERT_FALSE(success);
  ASSERT_FALSE(handler->ran());
}

}  // namespace update_client
