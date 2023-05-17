// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_MOCK_ORIGIN_TRIALS_SETTINGS_STORAGE_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_MOCK_ORIGIN_TRIALS_SETTINGS_STORAGE_H_

#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace embedder_support {

class MockOriginTrialsSettingsStorage : public OriginTrialsSettingsStorage {
 public:
  MockOriginTrialsSettingsStorage();
  ~MockOriginTrialsSettingsStorage() override;
  MOCK_METHOD(blink::mojom::OriginTrialsSettingsPtr,
              GetSettings,
              (),
              (const override));
  MOCK_METHOD(void,
              PopulateSettings,
              (const base::Value::List& disabled_tokens_list),
              (override));
};

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_MOCK_ORIGIN_TRIALS_SETTINGS_STORAGE_H_
