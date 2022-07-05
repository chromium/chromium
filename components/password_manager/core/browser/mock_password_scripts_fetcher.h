// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_SCRIPTS_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_SCRIPTS_FETCHER_H_

#include "components/password_manager/core/browser/password_scripts_fetcher.h"

#include "base/callback.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace password_manager {

class MockPasswordScriptsFetcher : public PasswordScriptsFetcher {
 public:
  explicit MockPasswordScriptsFetcher();
  ~MockPasswordScriptsFetcher() override;

  MOCK_METHOD(void, PrewarmCache, (), (override));

  MOCK_METHOD(void, RefreshScriptsIfNecessary, (base::OnceClosure), (override));

  MOCK_METHOD(void,
              FetchScriptAvailability,
              (const url::Origin&, base::OnceCallback<void(bool)>),
              (override));

  MOCK_METHOD(bool, IsScriptAvailable, (const url::Origin&), (const override));

  MOCK_METHOD(bool, IsCacheStale, (), (const override));

  MOCK_METHOD(base::Value::Dict,
              GetDebugInformationForInternals,
              (),
              (const override));

  MOCK_METHOD(base::Value::List, GetCacheEntries, (), (const override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_SCRIPTS_FETCHER_H_
