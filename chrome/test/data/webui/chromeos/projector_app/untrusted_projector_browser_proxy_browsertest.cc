// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"

namespace ash {
namespace {

constexpr std::string_view kOkJsonHeader =
    "HTTP/1.1 200 OK\nContent-type: application/json\n";
constexpr std::string_view kTestHeader = "X-Projector-Test";

bool OnUrlIntercepted(content::URLLoaderInterceptor::RequestParams* params) {
  if (params->url_request.headers.HasHeader(kTestHeader)) {
    content::URLLoaderInterceptor::WriteResponse(kOkJsonHeader, /*body=*/"",
                                                 params->client.get());
    return true;
  }
  return false;
}

class UntrustedProjectorBrowserProxyBrowserTest : public WebUIMochaBrowserTest {
 protected:
  UntrustedProjectorBrowserProxyBrowserTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(ash::kChromeUIProjectorAppHost);
  }
};

IN_PROC_BROWSER_TEST_F(UntrustedProjectorBrowserProxyBrowserTest, SendXhr) {
  auto url_loader_interceptor = std::make_unique<content::URLLoaderInterceptor>(
      base::BindRepeating(&OnUrlIntercepted));
  RunTestWithoutTestLoader(
      "chromeos/projector_app/untrusted_projector_browser_proxy_test.js",
      "mocha.run()");
}

}  // namespace
}  // namespace ash
