// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TEST_TEST_BROWSER_CONTEXT_WITH_PREFS_H_
#define COMPONENTS_USER_PREFS_TEST_TEST_BROWSER_CONTEXT_WITH_PREFS_H_

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_context.h"

namespace user_prefs {

class TestBrowserContextWithPrefs : public content::TestBrowserContext {
 public:
  TestBrowserContextWithPrefs();

  TestBrowserContextWithPrefs(const TestBrowserContextWithPrefs&) = delete;
  TestBrowserContextWithPrefs& operator=(const TestBrowserContextWithPrefs&) =
      delete;

  ~TestBrowserContextWithPrefs() override;

  PrefService* prefs() { return &prefs_; }
  PrefRegistrySimple* pref_registry() { return prefs_.registry(); }

 private:
  const raw_ptr<BrowserContextDependencyManager>
      browser_context_dependency_manager_;
  TestingPrefServiceSimple prefs_;
};

}  // namespace user_prefs

#endif  // COMPONENTS_USER_PREFS_TEST_TEST_BROWSER_CONTEXT_WITH_PREFS_H_
