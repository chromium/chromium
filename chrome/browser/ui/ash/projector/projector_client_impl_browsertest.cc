// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace ash {

class ProjectorClientTest : public InProcessBrowserTest {
 public:
  ProjectorClientTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kProjector);
  }

  ~ProjectorClientTest() override = default;
  ProjectorClientTest(const ProjectorClientTest&) = delete;
  ProjectorClientTest& operator=(const ProjectorClientTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    client_ = std::make_unique<ProjectorClientImpl>();
  }

 protected:
  std::unique_ptr<ProjectorClient> client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, ShowOrCloseSelfieCamTest) {
  EXPECT_FALSE(client_->IsSelfieCamVisible());
  client_->ShowSelfieCam();
  EXPECT_TRUE(client_->IsSelfieCamVisible());
  client_->CloseSelfieCam();
  EXPECT_FALSE(client_->IsSelfieCamVisible());
}

// TODO(crbug/1199396): Add a test to verify the selfie cam turns off when the
// device goes inactive.

}  // namespace ash
