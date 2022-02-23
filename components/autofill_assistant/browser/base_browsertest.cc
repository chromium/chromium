// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/base_browsertest.h"

#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"

namespace autofill_assistant {

// Flag to enable site per process to enforce OOPIFs.
const char* kSitePerProcess = "site-per-process";

BaseBrowserTest::BaseBrowserTest() = default;
BaseBrowserTest::~BaseBrowserTest() {}

void BaseBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(kSitePerProcess);
  // Necessary to avoid flakiness or failure due to input arriving
  // before the first compositor commit.
  command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
}

void BaseBrowserTest::SetUpOnMainThread() {
  ContentBrowserTest::SetUpOnMainThread();

  // Start a mock server for hosting an OOPIF.
  http_server_iframe_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTP);
  http_server_iframe_->ServeFilesFromSourceDirectory(
      "components/test/data/autofill_assistant/html_iframe");
  ASSERT_TRUE(http_server_iframe_->Start(8081));

  // Start the main server hosting the test page.
  http_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTP);
  http_server_->ServeFilesFromSourceDirectory(
      "components/test/data/autofill_assistant/html");
  ASSERT_TRUE(http_server_->Start(8080));
  ASSERT_TRUE(NavigateToURL(shell(), http_server_->GetURL(kTargetWebsitePath)));
}

}  // namespace autofill_assistant
