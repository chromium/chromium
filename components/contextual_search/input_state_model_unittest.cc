// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/input_state_model.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/searchbox_config.pb.h"

namespace contextual_search {

class InputStateModelTest : public testing::Test {
 public:
  InputStateModelTest() = default;
  ~InputStateModelTest() override = default;

  void SetUp() override {
    input_state_model_ = std::make_unique<InputStateModel>(
        session_handle_, omnibox::SearchboxConfig());
  }

 protected:
  std::unique_ptr<InputStateModel> input_state_model_;
  MockContextualSearchSessionHandle session_handle_;
};

TEST_F(InputStateModelTest, TestInitialization) {
  EXPECT_TRUE(input_state_model_);
}

TEST_F(InputStateModelTest, TestSubscribeAndNotify) {
  base::MockCallback<InputStateModel::Subscriber> mock_subscriber;
  base::CallbackListSubscription subscription =
      input_state_model_->subscribe(mock_subscriber.Get());

  EXPECT_CALL(mock_subscriber, Run(testing::_)).Times(1);
  // Setting a tool notifies subscribers.
  input_state_model_->setActiveTool(ToolMode::TOOL_MODE_UNSPECIFIED);
}

}  // namespace contextual_search
