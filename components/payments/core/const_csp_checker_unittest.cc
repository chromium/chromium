// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/const_csp_checker.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

TEST(ConstCSPCheckerTest, AlwaysAllow) {
  ConstCSPChecker checker(/*allow=*/true);
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  checker.AllowConnectToSource(
      /*url=*/GURL(), /*url_before_redirects=*/GURL(),
      /*did_follow_redirect=*/false,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, bool actual_allow) {
            EXPECT_TRUE(actual_allow);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();
}

TEST(ConstCSPCheckerTest, AlwaysDeny) {
  ConstCSPChecker checker(/*allow=*/false);
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  checker.AllowConnectToSource(
      /*url=*/GURL(), /*url_before_redirects=*/GURL(),
      /*did_follow_redirect=*/false,
      base::BindOnce(
          [](base::RepeatingClosure quit_closure, bool actual_allow) {
            EXPECT_FALSE(actual_allow);
            quit_closure.Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();
}

}  // namespace
}  // namespace payments
