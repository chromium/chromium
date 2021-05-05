// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_phishing_model.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(ClientSidePhishingModelTest, NotifiesOnUpdate) {
  content::BrowserTaskEnvironment task_environment;
  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      ClientSidePhishingModel::GetInstance()->RegisterCallback(
          base::BindRepeating(
              [](base::RepeatingClosure quit_closure, bool* called) {
                *called = true;
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), &called));

  ClientSideModel model;
  model.set_max_words_per_term(0);  // Required field
  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      model.SerializeAsString());

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_EQ(model.SerializeAsString(),
            ClientSidePhishingModel::GetInstance()->GetModelStr());
}

TEST(ClientSidePhishingModelTest, RejectsInvalidProto) {
  // Empty the model, just in case a previous test had successfully set it.
  ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      "bad proto");
  EXPECT_FALSE(ClientSidePhishingModel::GetInstance()->IsEnabled());
}

}  // namespace safe_browsing
