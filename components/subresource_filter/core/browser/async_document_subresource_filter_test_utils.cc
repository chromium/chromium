// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/async_document_subresource_filter_test_utils.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter::testing {

TestActivationStateCallbackReceiver::TestActivationStateCallbackReceiver() =
    default;
TestActivationStateCallbackReceiver::~TestActivationStateCallbackReceiver() =
    default;

base::OnceCallback<void(mojom::ActivationState)>
TestActivationStateCallbackReceiver::GetCallback() {
  return base::BindOnce(&TestActivationStateCallbackReceiver::Callback,
                        base::Unretained(this));
}

void TestActivationStateCallbackReceiver::WaitForActivationDecision() {
  ASSERT_EQ(0, callback_count_);
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestActivationStateCallbackReceiver::ExpectReceivedOnce(
    const mojom::ActivationState& expected_state) const {
  ASSERT_EQ(1, callback_count_);
  EXPECT_TRUE(expected_state.Equals(last_activation_state_));
}

void TestActivationStateCallbackReceiver::Callback(
    mojom::ActivationState activation_state) {
  ++callback_count_;
  last_activation_state_ = activation_state;
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

}  // namespace subresource_filter::testing
