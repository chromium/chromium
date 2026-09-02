// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_navigation_throttle.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/skills/features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::WithArg;

namespace {

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD(content::WebContents*,
              OpenURLFromTab,
              (content::WebContents*,
               const content::OpenURLParams&,
               base::OnceCallback<void(content::NavigationHandle&)>),
              (override));
};

class SkillsNavigationThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  SkillsNavigationThrottleTest() {
    feature_list_.InitAndEnableFeature(features::kSkillsWebViewV2Enabled);
  }

  bool CreatesThrottle(const GURL& url,
                       const url::Origin& initiator_origin,
                       ui::PageTransition transition = ui::PAGE_TRANSITION_LINK,
                       content::RenderFrameHost* rfh = nullptr) {
    content::MockNavigationHandle handle(url, rfh ? rfh : main_rfh());
    handle.set_page_transition(transition);
    handle.set_initiator_origin(initiator_origin);
    handle.set_is_renderer_initiated(true);

    content::MockNavigationThrottleRegistry registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    SkillsNavigationThrottle::MaybeCreateAndAdd(registry);
    return !registry.throttles().empty();
  }

  GURL DefaultUrl() { return GURL(features::kInterceptedSkillsUrl.Get()); }

  url::Origin DefaultOrigin() { return url::Origin::Create(DefaultUrl()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

struct RedirectionTestCase {
  const char* test_name;
  bool is_initial_navigation;
  WindowOpenDisposition expected_disposition;
};

class SkillsNavigationThrottleRedirectionTest
    : public SkillsNavigationThrottleTest,
      public ::testing::WithParamInterface<RedirectionTestCase> {};

TEST_P(SkillsNavigationThrottleRedirectionTest, InterceptsAndRedirects) {
  const RedirectionTestCase& param = GetParam();

  if (!param.is_initial_navigation) {
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("chrome://skills"));
  }
  ASSERT_EQ(param.is_initial_navigation,
            web_contents()->GetController().IsInitialNavigation());

  content::MockNavigationHandle handle(DefaultUrl(), main_rfh());
  handle.set_page_transition(ui::PAGE_TRANSITION_LINK);
  handle.set_initiator_origin(DefaultOrigin());
  handle.set_is_renderer_initiated(true);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  SkillsNavigationThrottle::MaybeCreateAndAdd(registry);

  ASSERT_EQ(registry.throttles().size(), 1u);
  content::NavigationThrottle* throttle = registry.throttles()[0].get();
  ASSERT_NE(throttle, nullptr);
  EXPECT_STREQ(throttle->GetNameForLogging(), "SkillsNavigationThrottle");

  MockWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OpenURLFromTab(web_contents(), _, _))
      .WillOnce(WithArg<1>([&](const content::OpenURLParams& params) {
        EXPECT_EQ(params.url, GURL(features::kSkillsSettingsPageUrl.Get()));
        EXPECT_EQ(params.disposition, param.expected_disposition);
        EXPECT_TRUE(ui::PageTransitionCoreTypeIs(params.transition,
                                                 ui::PAGE_TRANSITION_LINK));
        run_loop.Quit();
        return nullptr;
      }));

  EXPECT_EQ(throttle->WillStartRequest().action(),
            content::NavigationThrottle::CANCEL_AND_IGNORE);

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SkillsNavigationThrottleRedirectionTest,
    ::testing::Values(
        RedirectionTestCase{
            .test_name = "NewTabWhenPageCommitted",
            .is_initial_navigation = false,
            .expected_disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB,
        },
        RedirectionTestCase{
            .test_name = "CurrentTabOnInitialNavigation",
            .is_initial_navigation = true,
            .expected_disposition = WindowOpenDisposition::CURRENT_TAB,
        }),
    [](const ::testing::TestParamInfo<RedirectionTestCase>& info) {
      return info.param.test_name;
    });

struct CreationTestCase {
  const char* test_name;
  GURL url;
  url::Origin initiator_origin;
  ui::PageTransition transition = ui::PAGE_TRANSITION_LINK;
  bool should_create = false;
};

class SkillsNavigationThrottleCreationTest
    : public SkillsNavigationThrottleTest,
      public ::testing::WithParamInterface<CreationTestCase> {};

TEST_P(SkillsNavigationThrottleCreationTest, ChecksThrottleCreation) {
  const CreationTestCase& param = GetParam();
  EXPECT_EQ(
      param.should_create,
      CreatesThrottle(param.url, param.initiator_origin, param.transition));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SkillsNavigationThrottleCreationTest,
    ::testing::Values(
        // Valid triggers (true)
        CreationTestCase{
            .test_name = "WebClientOrigin",
            .url = GURL("https://clients5.google.com/chromeskills/"
                        "settings?utm_source=chrome-skills-settings"),
            .initiator_origin =
                url::Origin::Create(GURL("https://clients5.google.com")),
            .should_create = true,
        },
        CreationTestCase{
            .test_name = "WebClientOriginWithRef",
            .url = GURL("https://clients5.google.com/chromeskills/"
                        "settings?utm_source=chrome-skills-settings#section"),
            .initiator_origin =
                url::Origin::Create(GURL("https://clients5.google.com")),
            .should_create = true,
        },
        CreationTestCase{
            .test_name = "WebUIOrigin",
            .url = GURL("https://clients5.google.com/chromeskills/"
                        "settings?utm_source=chrome-skills-settings"),
            .initiator_origin = url::Origin::Create(GURL("chrome://skills")),
            .should_create = true,
        },
        CreationTestCase{
            .test_name = "UntrustedWebUIOrigin",
            .url = GURL("https://clients5.google.com/chromeskills/"
                        "settings?utm_source=chrome-skills-settings"),
            .initiator_origin =
                url::Origin::Create(GURL("chrome-untrusted://skills")),
            .should_create = true,
        },
        // Invalid triggers (false)
        CreationTestCase{
            .test_name = "InvalidPath",
            .url = GURL("https://clients5.google.com/wrong_path?"
                        "utm_source=chrome-skills-settings"),
            .initiator_origin =
                url::Origin::Create(GURL("https://clients5.google.com")),
            .should_create = false,
        },
        CreationTestCase{
            .test_name = "InvalidQueryParam",
            .url = GURL("https://clients5.google.com/chromeskills/"
                        "settings?utm_source=wrong"),
            .initiator_origin =
                url::Origin::Create(GURL("https://clients5.google.com")),
            .should_create = false,
        },
        CreationTestCase{
            .test_name = "MissingQueryParam",
            .url = GURL("https://clients5.google.com/chromeskills/settings"),
            .initiator_origin =
                url::Origin::Create(GURL("https://clients5.google.com")),
            .should_create = false,
        },
        CreationTestCase{
            .test_name = "MismatchedHost",
            .url = GURL("https://other-domain.com/chromeskills/"
                        "settings?utm_source=chrome-skills-settings"),
            .initiator_origin =
                url::Origin::Create(GURL("https://clients5.google.com")),
            .should_create = false,
        },
        CreationTestCase{
            .test_name = "MismatchedScheme",
            .url = GURL("http://clients5.google.com/chromeskills/"
                        "settings?utm_source=chrome-skills-settings"),
            .initiator_origin =
                url::Origin::Create(GURL("https://clients5.google.com")),
            .should_create = false,
        },
        CreationTestCase{
            .test_name = "InvalidInitiatorOrigin",
            .url = GURL("https://clients5.google.com/chromeskills/"
                        "settings?utm_source=chrome-skills-settings"),
            .initiator_origin =
                url::Origin::Create(GURL("https://example.com")),
            .should_create = false,
        },
        CreationTestCase{
            .test_name = "NonLinkTransition",
            .url = GURL("https://clients5.google.com/chromeskills/"
                        "settings?utm_source=chrome-skills-settings"),
            .initiator_origin =
                url::Origin::Create(GURL("https://clients5.google.com")),
            .transition = ui::PAGE_TRANSITION_TYPED,
            .should_create = false,
        }),
    [](const ::testing::TestParamInfo<CreationTestCase>& info) {
      return info.param.test_name;
    });

TEST_F(SkillsNavigationThrottleTest, CustomFinchUrlTriggersThrottle) {
  const GURL default_prod_url(
      "https://clients5.google.com/chromeskills/"
      "settings?utm_source=chrome-skills-settings");
  const url::Origin default_prod_origin = url::Origin::Create(default_prod_url);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kSkillsWebViewV2Enabled,
      {{"intercepted_skills_url",
        "http://localhost:8080/chromeskills/"
        "settings?utm_source=chrome-skills-settings"}});

  GURL custom_url(
      "http://localhost:8080/chromeskills/"
      "settings?utm_source=chrome-skills-settings");
  url::Origin custom_origin =
      url::Origin::Create(GURL("http://localhost:8080"));

  // Default production URL should not trigger when Finch overrides the URL.
  EXPECT_FALSE(CreatesThrottle(default_prod_url, default_prod_origin));

  // Custom Finch URL should trigger.
  EXPECT_TRUE(CreatesThrottle(custom_url, custom_origin));
}

TEST_F(SkillsNavigationThrottleTest, SubframeNavigationDoesNotTrigger) {
  GURL url = DefaultUrl();
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("child_frame");
  ASSERT_TRUE(child_rfh);

  EXPECT_FALSE(CreatesThrottle(url, DefaultOrigin(), ui::PAGE_TRANSITION_LINK,
                               child_rfh));
}

TEST_F(SkillsNavigationThrottleTest, FeatureDisabledDoesNotCreateThrottle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kSkillsWebViewV2Enabled);

  EXPECT_FALSE(CreatesThrottle(DefaultUrl(), DefaultOrigin()));
}

}  // namespace
