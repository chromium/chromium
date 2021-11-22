// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_agent.h"

#include "base/files/file.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/content/common/autofill_assistant_driver.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;

class MockAutofillAssistantDriver : public mojom::AutofillAssistantDriver {
 public:
  void BindHandle(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this, mojo::PendingReceiver<mojom::AutofillAssistantDriver>(
                             std::move(handle)));
  }

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    associated_receivers_.Add(
        this, mojo::PendingAssociatedReceiver<mojom::AutofillAssistantDriver>(
                  std::move(handle)));
  }

  MOCK_METHOD(void,
              GetAnnotateDomModel,
              (base::OnceCallback<void(base::File)> callback),
              (override));

 private:
  mojo::ReceiverSet<mojom::AutofillAssistantDriver> receivers_;
  mojo::AssociatedReceiverSet<mojom::AutofillAssistantDriver>
      associated_receivers_;
};

class AutofillAssistantAgentBrowserTest : public content::RenderViewTest {
 public:
  AutofillAssistantAgentBrowserTest() = default;

  void SetUp() override {
    RenderViewTest::SetUp();

    GetMainRenderFrame()->GetBrowserInterfaceBroker()->SetBinderForTesting(
        mojom::AutofillAssistantDriver::Name_,
        base::BindRepeating(&MockAutofillAssistantDriver::BindHandle,
                            base::Unretained(&autofill_assistant_driver_)));
    blink::AssociatedInterfaceProvider* remote_interfaces =
        GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::AutofillAssistantDriver::Name_,
        base::BindRepeating(&MockAutofillAssistantDriver::BindPendingReceiver,
                            base::Unretained(&autofill_assistant_driver_)));
    autofill_assistant_agent_ = std::make_unique<AutofillAssistantAgent>(
        GetMainRenderFrame(), &associated_interfaces_);
  }

  void TearDown() override {
    autofill_assistant_agent_.reset();
    RenderViewTest::TearDown();
  }

 protected:
  MockAutofillAssistantDriver autofill_assistant_driver_;
  std::unique_ptr<AutofillAssistantAgent> autofill_assistant_agent_;
  base::MockCallback<base::OnceCallback<void(base::File)>> callback_;

 private:
  blink::AssociatedInterfaceRegistry associated_interfaces_;
};

TEST_F(AutofillAssistantAgentBrowserTest, GetModelFile) {
  EXPECT_CALL(autofill_assistant_driver_, GetAnnotateDomModel)
      .WillOnce(RunOnceCallback<0>(base::File()));

  EXPECT_CALL(callback_, Run);
  autofill_assistant_agent_->GetAnnotateDomModel(callback_.Get());

  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace autofill_assistant
