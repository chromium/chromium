// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/test_server_redirect_handle.h"

#include "base/test/bind.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

TestServerRedirectHandle::TestServerRedirectHandle(
    net::EmbeddedTestServer& test_server) {
  test_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&, this](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        GURL request_url =
            params_.origin
                ? test_server.GetURL(params_.origin, request.relative_url)
                : request.GetURL();
        if (request_url == params_.redirect_url) {
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(params_.redirect_code);
          http_response->AddCustomHeader("Location", params_.target_url.spec());
          return http_response;
        }
        return nullptr;
      }));
}

base::AutoReset<TestServerRedirectHandle::Params>
TestServerRedirectHandle::Redirect(Params params) {
  return base::AutoReset<Params>(&params_, std::move(params));
}

}  // namespace web_app
