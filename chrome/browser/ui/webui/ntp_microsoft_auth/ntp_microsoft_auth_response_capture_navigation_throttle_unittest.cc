// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_response_capture_navigation_throttle.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/google/core/common/google_switches.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::WithArg;

namespace {

constexpr char kEnrollmentFallbackUrl[] =
    "https://chromeenterprise.google/ntp-microsoft-auth";
constexpr char kEntraLoginHost[] = "https://login.microsoftonline.com";

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD3(OpenURLFromTab,
               content::WebContents*(
                   content::WebContents*,
                   const content::OpenURLParams&,
                   base::OnceCallback<void(content::NavigationHandle&)>));
};

}  // namespace

class NtpMicrosoftAuthResponseCaptureNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  }

  std::unique_ptr<content::NavigationThrottle> CreateNavigationThrottle(
      const GURL& url,
      content::RenderFrameHost* render_frame_host,
      bool set_redirect_chain) {
    navigation_handle_ =
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            url, render_frame_host);
    if (set_redirect_chain) {
      std::vector<GURL> redirect_chain{GURL(kEntraLoginHost)};
      navigation_handle_->set_redirect_chain(redirect_chain);
    }
    navigation_throttle_registry_ =
        std::make_unique<content::MockNavigationThrottleRegistry>(
            navigation_handle_.get(),
            content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    NtpMicrosoftAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(
        *navigation_throttle_registry_);
    if (navigation_throttle_registry_->throttles().size() == 0) {
      return nullptr;
    }
    EXPECT_EQ(navigation_throttle_registry_->throttles().size(), 1u);
    return std::move(navigation_throttle_registry_->throttles().back());
  }

  void SetThirdPartyNTP() {
    ntp_test_utils::SetUserSelectedDefaultSearchProvider(
        profile(), "https://foo.com/", "https://foo.com/");
  }

  // Navigate opener to expected chrome-untrusted:// URL and set it as the
  // opener for the current web contents.
  void SetWebContentsWithOpener(content::WebContents* opener_contents) {
    content::WebContentsTester::For(opener_contents)
        ->NavigateAndCommit(
            GURL(chrome::kChromeUIUntrustedNtpMicrosoftAuthURL));
    content::WebContentsTester::For(web_contents())->SetOpener(opener_contents);
  }

  // Navigate current render frame to expected chrome-untrusted:// URL and
  // return a child frame.
  content::RenderFrameHost* CreateChildRenderFrame() {
    NavigateAndCommit(GURL(chrome::kChromeUIUntrustedNtpMicrosoftAuthURL));
    auto* parent_host_tester = content::RenderFrameHostTester::For(main_rfh());
    parent_host_tester->InitializeRenderFrameIfNeeded();
    return parent_host_tester->AppendChild("test");
  }

 private:
  std::unique_ptr<content::MockNavigationHandle> navigation_handle_;
  std::unique_ptr<content::MockNavigationThrottleRegistry>
      navigation_throttle_registry_;
};

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       DoNotCreateThrottleForNonFirstPartyNTP) {
  SetThirdPartyNTP();
  std::unique_ptr<content::WebContents> opener_contents =
      CreateTestWebContents();
  SetWebContentsWithOpener(opener_contents.get());
  auto throttle =
      CreateNavigationThrottle(GURL(kEnrollmentFallbackUrl), main_rfh(), false);

  EXPECT_FALSE(throttle);
}

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       DoNotCreateThrottleIfNotOpenedByNTP) {
  auto throttle =
      CreateNavigationThrottle(GURL(kEnrollmentFallbackUrl), main_rfh(), false);

  EXPECT_FALSE(throttle);
}

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       CreateThrottleForPopup) {
  std::unique_ptr<content::WebContents> opener_contents =
      CreateTestWebContents();
  SetWebContentsWithOpener(opener_contents.get());
  auto throttle =
      CreateNavigationThrottle(GURL(kEnrollmentFallbackUrl), main_rfh(), false);

  EXPECT_TRUE(throttle);
}

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       CreateThrottleForIFrame) {
  auto* child_frame_host = CreateChildRenderFrame();
  auto throttle = CreateNavigationThrottle(GURL(kEnrollmentFallbackUrl),
                                           child_frame_host, false);

  EXPECT_TRUE(throttle);
}

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       ProceedIfUnexpectedUrlPopup) {
  std::unique_ptr<content::WebContents> opener_contents =
      CreateTestWebContents();
  SetWebContentsWithOpener(opener_contents.get());
  auto throttle =
      CreateNavigationThrottle(GURL("https://foo.com"), main_rfh(), true);

  EXPECT_TRUE(throttle);
  EXPECT_EQ(NtpMicrosoftAuthResponseCaptureNavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
}

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       ProceedIfUnexpectedUrlIframe) {
  auto* child_frame_host = CreateChildRenderFrame();
  auto throttle =
      CreateNavigationThrottle(GURL("https://foo.com"), child_frame_host, true);

  EXPECT_TRUE(throttle);
  EXPECT_EQ(NtpMicrosoftAuthResponseCaptureNavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
}

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       CancelIfUnexpectedRedirectChainPopup) {
  std::unique_ptr<content::WebContents> opener_contents =
      CreateTestWebContents();
  SetWebContentsWithOpener(opener_contents.get());
  auto throttle =
      CreateNavigationThrottle(GURL(kEnrollmentFallbackUrl), main_rfh(), false);

  EXPECT_TRUE(throttle);
  EXPECT_EQ(
      NtpMicrosoftAuthResponseCaptureNavigationThrottle::CANCEL_AND_IGNORE,
      throttle->WillRedirectRequest());
}

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       CancelIfUnexpectedRedirectChainIframe) {
  auto* child_frame_host = CreateChildRenderFrame();
  auto throttle = CreateNavigationThrottle(GURL(kEnrollmentFallbackUrl),
                                           child_frame_host, false);

  EXPECT_TRUE(throttle);
  EXPECT_EQ(
      NtpMicrosoftAuthResponseCaptureNavigationThrottle::CANCEL_AND_IGNORE,
      throttle->WillRedirectRequest());
}

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       RedirectToAboutBlankPopup) {
  std::unique_ptr<content::WebContents> opener_contents =
      CreateTestWebContents();
  SetWebContentsWithOpener(opener_contents.get());
  auto web_contents_delegate = std::make_unique<MockWebContentsDelegate>();
  web_contents()->SetDelegate(web_contents_delegate.get());

  GURL url;
  content::SiteInstanceId opener_site_instance_id;
  std::optional<url::Origin> opener_origin;
  EXPECT_CALL(*web_contents_delegate, OpenURLFromTab)
      .Times(1)
      .WillOnce(WithArg<1>([&](const content::OpenURLParams& params) {
        url = params.url;
        opener_site_instance_id = params.source_site_instance->GetId();
        opener_origin = params.initiator_origin;
        return nullptr;
      }));

  // Add URL Fragment to navigation to ensure it is copied over to the
  // about:blank navigation.
  GURL::Replacements addRef;
  addRef.SetRefStr("code=1234");
  auto throttle = CreateNavigationThrottle(
      GURL(kEnrollmentFallbackUrl).ReplaceComponents(addRef), main_rfh(), true);

  EXPECT_TRUE(throttle);
  EXPECT_EQ(
      NtpMicrosoftAuthResponseCaptureNavigationThrottle::CANCEL_AND_IGNORE,
      throttle->WillRedirectRequest());

  // Wait for the task to navigate to about:blank to finish.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(url, GURL("about:blank#code=1234"));
  EXPECT_EQ(opener_site_instance_id,
            web_contents()->GetOpener()->GetSiteInstance()->GetId());
  EXPECT_EQ(opener_origin,
            web_contents()->GetOpener()->GetLastCommittedOrigin());
}

TEST_F(NtpMicrosoftAuthResponseCaptureNavigationThrottleTest,
       RedirectToAboutBlankIframe) {
  auto* child_frame_host = CreateChildRenderFrame();
  auto web_contents_delegate = std::make_unique<MockWebContentsDelegate>();
  web_contents()->SetDelegate(web_contents_delegate.get());

  GURL url;
  content::SiteInstanceId parent_site_instance_id;
  std::optional<url::Origin> parent_origin;
  EXPECT_CALL(*web_contents_delegate, OpenURLFromTab)
      .Times(1)
      .WillOnce(WithArg<1>([&](const content::OpenURLParams& params) {
        url = params.url;
        parent_site_instance_id = params.source_site_instance->GetId();
        parent_origin = params.initiator_origin;
        return nullptr;
      }));

  // Add URL Fragment to navigation to ensure it is copied over to the
  // about:blank navigation.
  GURL::Replacements addRef;
  addRef.SetRefStr("code=1234");
  auto throttle = CreateNavigationThrottle(
      GURL(kEnrollmentFallbackUrl).ReplaceComponents(addRef), child_frame_host,
      true);

  EXPECT_TRUE(throttle);
  EXPECT_EQ(
      NtpMicrosoftAuthResponseCaptureNavigationThrottle::CANCEL_AND_IGNORE,
      throttle->WillRedirectRequest());

  // Wait for the task to navigate to about:blank to finish.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(url, GURL("about:blank#code=1234"));
  EXPECT_EQ(parent_site_instance_id, main_rfh()->GetSiteInstance()->GetId());
  EXPECT_EQ(parent_origin, main_rfh()->GetLastCommittedOrigin());
}
