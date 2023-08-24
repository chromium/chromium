// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "components/browser_ui/client_certificate/android/ssl_client_certificate_request.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace browser_ui {
namespace {

constexpr char kOriginHostname[] = "a.test";

class SSLClientCertPendingRequestsPrerenderTest
    : public content::ContentBrowserTest {
 protected:
  SSLClientCertPendingRequestsPrerenderTest()
      : prerender_helper_(base::BindRepeating(
            &SSLClientCertPendingRequestsPrerenderTest::web_contents,
            base::Unretained(this))) {
    prerender_helper_.RegisterServerRequestMonitor(&https_server_);
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule(kOriginHostname, "127.0.0.1");

    content::ShellContentBrowserClient::Get()
        ->set_select_client_certificate_callback(
            base::BindOnce(&SSLClientCertPendingRequestsPrerenderTest::
                               OnSelectClientCertificate,
                           base::Unretained(this)));

    // Start the main server hosting the test page.
    https_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server_.Start());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
    ContentBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  base::OnceClosure OnSelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) {
    auto cancellation_closure = ShowSSLClientCertificateSelector(
        web_contents, cert_request_info, std::move(delegate));

    if (quit_closure_)
      std::move(quit_closure_).Run();

    return cancellation_closure;
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  content::test::PrerenderTestHelper prerender_helper_;
  base::OnceClosure quit_closure_;
};

// Verifies that a prerendering navigation will not reset the the client
// certificate dialog counter.
IN_PROC_BROWSER_TEST_F(SSLClientCertPendingRequestsPrerenderTest,
                       PrerendersDontResetCertRequestState) {
  const GURL initial_url =
      https_server().GetURL(kOriginHostname, "/title1.html");

  // Navigate to an initial page.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), initial_url));

  // Set to request a client certificate.
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server().ResetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                                ssl_config);

  // Add an iframe.
  EXPECT_TRUE(ExecJs(
      web_contents(),
      content::JsReplace("var iframe = document.createElement('iframe'); "
                         "iframe.src = $1; "
                         "document.body.appendChild(iframe);",
                         initial_url)));
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());
  run_loop.Run();

  // Check that the client certificate's dialog has been shown.
  EXPECT_EQ(1u,
            GetCountOfSSLClientCertificateSelectorForTesting(web_contents()));

  // Reset do not request the client certificate.
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::NO_CLIENT_CERT;
  https_server().ResetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES,
                                ssl_config);

  // Load a page in the prerender.
  const GURL prerendering_url =
      https_server().GetURL(kOriginHostname, "/empty.html");
  prerender_helper().AddPrerender(prerendering_url);

  // Check that the prerending navigation did not affect the client
  // certificate's dialog count.
  EXPECT_EQ(1u,
            GetCountOfSSLClientCertificateSelectorForTesting(web_contents()));
}

}  // namespace
}  // namespace browser_ui
