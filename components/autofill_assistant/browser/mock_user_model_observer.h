// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_USER_MODEL_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_USER_MODEL_OBSERVER_H_

#include <string>
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockUserModelObserver : public UserModel::Observer {
 public:
  MockUserModelObserver();
  ~MockUserModelObserver() override;

  MOCK_METHOD2(OnValueChanged,
               void(const std::string& identifier,
                    const ValueProto& new_value));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_USER_MODEL_OBSERVER_H_
