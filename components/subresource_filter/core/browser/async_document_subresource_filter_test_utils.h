// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_ASYNC_DOCUMENT_SUBRESOURCE_FILTER_TEST_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_ASYNC_DOCUMENT_SUBRESOURCE_FILTER_TEST_UTILS_H_

#include "base/functional/callback.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

namespace subresource_filter::testing {

// This test class is intended to be used in conjunction with an
// AsyncDocumentSubresourceFilter, and can be used to expect a certain
// activation result occured.
class TestActivationStateCallbackReceiver {
 public:
  TestActivationStateCallbackReceiver();

  TestActivationStateCallbackReceiver(
      const TestActivationStateCallbackReceiver&) = delete;
  TestActivationStateCallbackReceiver& operator=(
      const TestActivationStateCallbackReceiver&) = delete;

  ~TestActivationStateCallbackReceiver();

  base::OnceCallback<void(mojom::ActivationState)> GetCallback();
  void WaitForActivationDecision();
  void ExpectReceivedOnce(const mojom::ActivationState& expected_state) const;

  int callback_count() const { return callback_count_; }

 private:
  void Callback(mojom::ActivationState activation_state);

  mojom::ActivationState last_activation_state_;
  int callback_count_ = 0;

  base::OnceClosure quit_closure_;
};

}  // namespace subresource_filter::testing

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_ASYNC_DOCUMENT_SUBRESOURCE_FILTER_TEST_UTILS_H_
