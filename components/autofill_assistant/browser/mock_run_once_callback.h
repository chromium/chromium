// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_RUN_ONCE_CALLBACK_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_RUN_ONCE_CALLBACK_H_

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

// Templates for calling base::OnceCallback from gmock actions.
//
// To work around the fact that OnceCallback can't be copied, the method
// to be mocked needs to take the callback as a reference. To do it without
// changing the original interface, follow this pattern:
//
//   void DoSomething(..., base::OnceCallback<void(bool)> callback) override {
//     OnDoSomething(..., callback);
//   }
//   MOCK_METHOD2(OnDoSomething,
//       void(..., base::OnceCallback<void(bool)>& callback));
//
//

ACTION_TEMPLATE(RunOnceCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  return std::move(std::get<k>(args)).Run();
}

ACTION_TEMPLATE(RunOnceCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  return std::move(std::get<k>(args)).Run(p0);
}

ACTION_TEMPLATE(RunOnceCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, p1)) {
  return std::move(std::get<k>(args)).Run(p0, p1);
}

ACTION_TEMPLATE(RunOnceCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_3_VALUE_PARAMS(p0, p1, p2)) {
  return std::move(std::get<k>(args)).Run(p0, p1, p2);
}

// Template for capturing a base::OnceCallback passed to a mocked method
//
// This is useful to run the callback later on, at an appropriate time.
//
// base::OnceCallback<void(bool)> captured_callback;
//   EXPECT_CALL(my_mock_, MyMethod(_))
//     .WillOnce(CaptureOnceCallback<0>(&captured_callback));
// [...]
// std::move(captured_callback).Run();
//

ACTION_TEMPLATE(CaptureOnceCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  *p0 = std::move(std::get<k>(args));
}

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_RUN_ONCE_CALLBACK_H_
