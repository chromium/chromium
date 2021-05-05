// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic.h"

#include "base/memory/ref_counted.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class StarterHeuristicTest : public testing::Test {
 public:
  StarterHeuristicTest()
      : starter_heuristic_(base::MakeRefCounted<StarterHeuristic>()) {}

  // Synchronous evaluation of the heuristic for easier testing.
  bool IsHeuristicMatchForTest(const GURL& url) {
    return starter_heuristic_->IsHeuristicMatch(url);
  }

 protected:
  scoped_refptr<StarterHeuristic> starter_heuristic_;
};

TEST_F(StarterHeuristicTest, SmokeTest) {
  EXPECT_TRUE(IsHeuristicMatchForTest(GURL("https://www.example.com/cart")));
  EXPECT_FALSE(IsHeuristicMatchForTest(GURL("https://www.example.com")));
}

TEST_F(StarterHeuristicTest, RunHeuristicAsync) {
  base::test::TaskEnvironment task_environment;
  base::MockCallback<base::OnceCallback<void(bool result)>> callback;
  EXPECT_CALL(callback, Run(true));
  starter_heuristic_->RunHeuristicAsync(GURL("https://www.example.com/cart"),
                                        callback.Get());
  task_environment.RunUntilIdle();
}

}  // namespace autofill_assistant
