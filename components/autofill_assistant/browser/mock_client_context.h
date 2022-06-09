// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_CONTEXT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_CONTEXT_H_

#include "components/autofill_assistant/browser/client_context.h"

#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockClientContext : public ClientContext {
 public:
  MockClientContext();
  ~MockClientContext() override;

  MOCK_METHOD(void,
              Update,
              (const TriggerContext& trigger_context),
              (override));
  MOCK_METHOD(void,
              UpdateAnnotateDomModelContext,
              (int64_t model_version),
              (override));
  MOCK_METHOD(void,
              UpdateJsFlowLibraryLoaded,
              (bool js_flow_library_loaded),
              (override));
  MOCK_METHOD(ClientContextProto, AsProto, (), (const override));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_CLIENT_CONTEXT_H_
