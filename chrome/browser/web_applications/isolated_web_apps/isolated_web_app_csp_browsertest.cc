// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"

namespace web_app {

class IsolatedWebAppCspBrowserTest : public IsolatedWebAppBrowserTestHarness {
 public:
  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();

    embedded_https_test_server().ServeFilesFromDirectory(resource_path());
    ASSERT_TRUE(embedded_https_test_server().Start());

    embedded_test_server()->ServeFilesFromDirectory(resource_path());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  base::FilePath resource_path() {
    base::FilePath base_path;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path));
    return base_path.Append(GetChromeTestDataDir())
        .AppendASCII("web_apps/isolated_csp");
  }
};

// Tests that the <base> element is not allowed.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppCspBrowserTest, Base) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();

  app->TrustSigningKey();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  EXPECT_EQ("violation", EvalJs(app_frame, R"(
      new Promise(resolve => {
        document.addEventListener('securitypolicyviolation', e => {
          resolve('violation');
        });

        let base = document.createElement('base');
        base.href = '/test';
        document.body.appendChild(base);
      })
  )"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppCspBrowserTest, Src) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddFolderFromDisk("/", resource_path())
          .BuildBundle();

  app->TrustSigningKey();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  enum class OriginType {
    kSameOrigin,
    kHttp,
    kHttps,
  };
  struct {
    std::string element_name;
    OriginType origin_type;
    std::string path;
    std::string expectation;
  } test_cases[] = {
      // Cross-origin HTTPS images and media are allowed (but need a
      // Cross-Origin-Resource-Policy header, and will error otherwise)
      {"img", OriginType::kSameOrigin, "/single_face.jpg", "allowed"},
      {"img", OriginType::kHttp, "/single_face_corp.jpg", "violation"},
      {"img", OriginType::kHttps, "/single_face.jpg", "error"},
      {"img", OriginType::kHttps, "/single_face_corp.jpg", "allowed"},
      {"audio", OriginType::kSameOrigin, "/noise_corp.wav", "allowed"},
      {"audio", OriginType::kHttp, "/noise_corp.wav", "violation"},
      {"audio", OriginType::kHttps, "/noise.wav", "error"},
      {"audio", OriginType::kHttps, "/noise_corp.wav", "allowed"},
      {"video", OriginType::kSameOrigin, "/bear.webm", "allowed"},
      {"video", OriginType::kHttp, "/bear_corp.webm", "violation"},
      {"video", OriginType::kHttps, "/bear.webm", "error"},
      {"video", OriginType::kHttps, "/bear_corp.webm", "allowed"},
      // Plugins are disabled.
      {"embed", OriginType::kSameOrigin, "/favicon/icon.png", "violation"},
      // Iframes can contain cross-origin HTTPS content.
      {"iframe", OriginType::kSameOrigin, "/empty.html", "allowed"},
      {"iframe", OriginType::kHttp, "/empty.html", "violation"},
      {"iframe", OriginType::kHttps, "/empty.html", "allowed"},
      // Script tags must be same-origin.
      {"script", OriginType::kSameOrigin, "/empty_script.js", "allowed"},
      {"script", OriginType::kHttps, "/empty_script.js", "violation"},
      // Stylesheets must be same-origin as per style-src CSP.
      {"link", OriginType::kSameOrigin, "/empty_style.css", "allowed"},
      {"link", OriginType::kHttps, "/empty_style.css", "violation"},
  };

  for (const auto& test_case : test_cases) {
    url::Origin origin;
    switch (test_case.origin_type) {
      case OriginType::kSameOrigin:
        origin = url_info.origin();
        break;
      case OriginType::kHttp:
        origin = embedded_test_server()->GetOrigin("example.com");
        break;
      case OriginType::kHttps:
        origin = embedded_https_test_server().GetOrigin("example.com");
        break;
    }
    GURL src = origin.GetURL().Resolve(test_case.path);
    std::string test_js = content::JsReplace(R"(
        const policy = window.trustedTypes.createPolicy('policy', {
          createScriptURL: url => url,
        });

        new Promise(resolve => {
          document.addEventListener('securitypolicyviolation', e => {
            resolve('violation');
          });

          let element = document.createElement($1);
          if($1 === 'link') {
            // Stylesheets require `rel` and `href` instead of `src` to work.
            element.rel = 'stylesheet';
            element.href = $2;
          } else {
            // Not all elements being tested require Trusted Types, but
            // passing src through the policy for all elements works.
            element.src = policy.createScriptURL($2);
          }
          element.addEventListener('canplay', () => resolve('allowed'));
          element.addEventListener('load', () => resolve('allowed'));
          element.addEventListener('error', e => resolve('error'));
          document.body.appendChild(element);
        })
      )",
                                             test_case.element_name, src);
    SCOPED_TRACE(testing::Message() << "Running testcase: "
                                    << test_case.element_name << " " << src);
    EXPECT_EQ(test_case.expectation, EvalJs(app_frame, test_js));
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppCspBrowserTest, TrustedTypes) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddHtml("/script.js", "console.log('test');")
          .BuildBundle();

  app->TrustSigningKey();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  EXPECT_EQ("exception", EvalJs(app_frame, R"(
    new Promise(resolve => {
      document.addEventListener('securitypolicyviolation', e => {
        resolve('violation');
      });

      try {
        let element = document.createElement('script');
        element.src = '/script.js';
        element.addEventListener('load', () => resolve('allowed'));
        element.addEventListener('error', e => resolve('error'));
        document.body.appendChild(element);
      } catch (e) {
        resolve('exception');
      }
    })
  )"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppCspBrowserTest, Wasm) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();

  app->TrustSigningKey();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  EXPECT_EQ("allowed", EvalJs(app_frame, R"(
    new Promise(async (resolve) => {
      document.addEventListener('securitypolicyviolation', e => {
        resolve('violation');
      });

      try {
        await WebAssembly.compile(new Uint8Array(
            // The smallest possible Wasm module. Just the header
            // (0, "A", "S", "M"), and the version (0x1).
            [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
        resolve('allowed');
      } catch (e) {
        resolve('exception: ' + e);
      }
    })
  )"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppCspBrowserTest, UnsafeInlineStyleSrc) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();

  app->TrustSigningKey();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  EXPECT_EQ("none", EvalJs(app_frame, R"(
    new Promise(async (resolve) => {
      document.addEventListener('securitypolicyviolation', e => {
        resolve('violation');
      });

      try {
        document.body.setAttribute("style", "display: none;");
        const bodyStyles = window.getComputedStyle(document.body);
        resolve(bodyStyles.getPropertyValue("display"));
      } catch (e) {
        resolve('exception: ' + e);
      }
    })
  )"));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppCspBrowserTest, ProxyMode) {
  std::unique_ptr<ScopedProxyIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder())
          .AddResource("/", "no script page",
                       {{"Content-Type", "text/html"},
                        {"Content-Security-Policy", "script-src 'none'"}})
          .AddJs("/script.js", "console.log('hello world')")
          .BuildAndStartProxyServer();

  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL src = url_info.origin().GetURL().Resolve("/script.js");
  std::string test_js = content::JsReplace(R"(
      const policy = window.trustedTypes.createPolicy('policy', {
        createScriptURL: url => url,
      });

      new Promise(resolve => {
        document.addEventListener('securitypolicyviolation', e => {
          resolve('violation');
        });

        let element = document.createElement('script');
        element.src = policy.createScriptURL($1);
        element.addEventListener('load', () => resolve('allowed'));
        element.addEventListener('error', e => resolve('error'));
        document.body.appendChild(element);
      })
    )",
                                           src);
  EXPECT_EQ("violation", EvalJs(app_frame, test_js));
}

struct WebSocketTestParam {
  net::SpawnedTestServer::Type type;
  std::string expected_result;
};

class IsolatedWebAppWebSocketCspBrowserTest
    : public IsolatedWebAppCspBrowserTest,
      public testing::WithParamInterface<WebSocketTestParam> {};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppWebSocketCspBrowserTest, CheckCsp) {
  auto websocket_test_server = std::make_unique<net::SpawnedTestServer>(
      GetParam().type, net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(websocket_test_server->Start());

  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();

  app->TrustSigningKey();
  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info, app->Install(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  // The |websocket_url| will echo the message we send to it.
  GURL websocket_url = websocket_test_server->GetURL("echo-with-no-extension");

  EXPECT_EQ(GetParam().expected_result,
            EvalJs(app_frame, content::JsReplace(R"(
    new Promise(async (resolve) => {
      document.addEventListener('securitypolicyviolation', e => {
        resolve('violation');
      });

      try {
        new WebSocket($1).onopen = () => resolve('allowed');
      } catch (e) {
        resolve('exception: ' + e);
      }
    })
  )",
                                                 websocket_url)));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IsolatedWebAppWebSocketCspBrowserTest,
    ::testing::Values(
        WebSocketTestParam{.type = net::SpawnedTestServer::TYPE_WS,
                           .expected_result = "violation"},
        WebSocketTestParam{.type = net::SpawnedTestServer::TYPE_WSS,
                           .expected_result = "allowed"}),
    [](const testing::TestParamInfo<
        IsolatedWebAppWebSocketCspBrowserTest::ParamType>& info)
        -> std::string {
      switch (info.param.type) {
        case net::SpawnedTestServer::TYPE_WS:
          return "Ws";
        case net::SpawnedTestServer::TYPE_WSS:
          return "Wss";
        default:
          NOTREACHED();
      }
    });

}  // namespace web_app
