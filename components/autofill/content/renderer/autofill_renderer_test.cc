// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_renderer_test.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::test {

MockAutofillDriver::MockAutofillDriver() = default;

MockAutofillDriver::~MockAutofillDriver() = default;

AutofillRendererTest::AutofillRendererTest() = default;

AutofillRendererTest::~AutofillRendererTest() = default;

void AutofillRendererTest::SetUp() {
  RenderViewTest::SetUp();

  blink::AssociatedInterfaceProvider* remote_interfaces =
      GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::AutofillDriver::Name_,
      base::BindRepeating(&MockAutofillDriver::BindPendingReceiver,
                          base::Unretained(&autofill_driver_)));

  auto password_autofill_agent = std::make_unique<TestPasswordAutofillAgent>(
      GetMainRenderFrame(), &associated_interfaces_);
  auto password_generation = std::make_unique<PasswordGenerationAgent>(
      GetMainRenderFrame(), password_autofill_agent.get(),
      &associated_interfaces_);
  autofill_agent_ = std::make_unique<AutofillAgent>(
      GetMainRenderFrame(), std::move(password_autofill_agent),
      std::move(password_generation), &associated_interfaces_);
}

void AutofillRendererTest::TearDown() {
  // Explicitly set the `AutofillClient` to null before resetting the agent -
  // otherwise the frame has a dangling pointer and document unloading may
  // cause a UAF.
  GetMainFrame()->SetAutofillClient(nullptr);
  autofill_agent_.reset();
  RenderViewTest::TearDown();
}

}  // namespace autofill::test
