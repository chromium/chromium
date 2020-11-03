// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script_coordinator.h"

#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/service/mock_service_request_sender.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::NiceMock;
using ::testing::Return;

namespace {

class MockObserver : public TriggerScriptCoordinator::Observer {
 public:
  MOCK_METHOD1(OnTriggerScriptShown, void(const TriggerScriptUIProto& proto));
  MOCK_METHOD0(OnTriggerScriptHidden, void());
  MOCK_METHOD1(OnTriggerScriptFinished, void(int state));
};

const char kFakeServerUrl[] =
    "https://www.fake.backend.com/trigger_script_server";

class TriggerScriptCoordinatorTest : public content::RenderViewHostTestHarness {
 public:
  TriggerScriptCoordinatorTest() = default;
  ~TriggerScriptCoordinatorTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ON_CALL(mock_client_, GetWebContents).WillByDefault(Return(web_contents()));

    auto mock_request_sender =
        std::make_unique<NiceMock<MockServiceRequestSender>>();
    mock_request_sender_ = mock_request_sender.get();

    auto mock_web_controller = std::make_unique<NiceMock<MockWebController>>();
    mock_web_controller_ = mock_web_controller.get();

    coordinator_ = std::make_unique<TriggerScriptCoordinator>(
        &mock_client_, std::move(mock_web_controller),
        std::move(mock_request_sender), GURL(kFakeServerUrl));
    coordinator_->AddObserver(&mock_observer_);
  }

  void TearDown() override { coordinator_->RemoveObserver(&mock_observer_); }

 protected:
  NiceMock<MockServiceRequestSender>* mock_request_sender_;
  NiceMock<MockWebController>* mock_web_controller_;
  NiceMock<MockClient> mock_client_;
  NiceMock<MockObserver> mock_observer_;
  std::unique_ptr<TriggerScriptCoordinator> coordinator_;
};

TEST_F(TriggerScriptCoordinatorTest, SmokeTest) {
  // stub
}

}  // namespace
}  // namespace autofill_assistant
