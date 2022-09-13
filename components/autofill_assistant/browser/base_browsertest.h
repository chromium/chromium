// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BASE_BROWSERTEST_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BASE_BROWSERTEST_H_

#include <memory>

#include "content/public/test/content_browser_test.h"

namespace autofill_assistant {

class BaseBrowserTest : public content::ContentBrowserTest {
 public:
  const char* kTargetWebsitePath = "/autofill_assistant_target_website.html";

  BaseBrowserTest();
  ~BaseBrowserTest() override;

  BaseBrowserTest(const BaseBrowserTest&) = delete;
  BaseBrowserTest& operator=(const BaseBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 private:
  std::unique_ptr<net::EmbeddedTestServer> http_server_;
  std::unique_ptr<net::EmbeddedTestServer> http_server_iframe_;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BASE_BROWSERTEST_H_
