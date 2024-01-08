// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_EXTENSION_JS_BROWSER_TEST_H_
#define CHROME_TEST_BASE_ASH_EXTENSION_JS_BROWSER_TEST_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/devtools_agent_coverage_observer.h"
#include "chrome/test/base/ash/javascript_browser_test.h"

// A super class that handles javascript-based tests against an extension.
//
// See an example usage at
// chrome/browser/resources/chromeos/accessibility/chromevox/background/background_test.js
class ExtensionJSBrowserTest : public JavaScriptBrowserTest {
 public:
  ExtensionJSBrowserTest();
  ExtensionJSBrowserTest(const ExtensionJSBrowserTest&) = delete;
  ExtensionJSBrowserTest& operator=(const ExtensionJSBrowserTest&) = delete;
  ~ExtensionJSBrowserTest() override;

 protected:
  void SetUpOnMainThread() override;

  // Waits for an extension to load; returns immediately if already loaded.
  void WaitForExtension(const char* extension_id, base::OnceClosure load_cb);

  // Method required for js2gtest.
  // Runs |test_fixture|.|test_name| using the framework in test_api.js.
  bool RunJavascriptTestF(bool is_async,
                          const std::string& test_fixture,
                          const std::string& test_name);

 private:
  // The ID of the extension loaded in WaitForExtension().
  std::string extension_id_;
  // The browser context associated with the ExtensionHost loaded from
  // WaitForExtension().
  raw_ptr<content::BrowserContext, DanglingUntriaged>
      extension_host_browser_context_ = nullptr;
  bool libs_loaded_ = false;

  // Handles collection of code coverage.
  std::unique_ptr<DevToolsAgentCoverageObserver> coverage_handler_;
};

#endif  // CHROME_TEST_BASE_ASH_EXTENSION_JS_BROWSER_TEST_H_
