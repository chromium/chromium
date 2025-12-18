// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

namespace {

struct ProfilePickerSignInTestParam {
  PixelTestParam pixel_test_param;
};

std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<ProfilePickerSignInTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

const ProfilePickerSignInTestParam kTestParams[] = {
    {.pixel_test_param = {.test_suffix = "Regular"}},
    {.pixel_test_param = {.test_suffix = "DarkTheme", .use_dark_theme = true}},
};

const char kMockGaiaHtml[] = R"(
  <!DOCTYPE html><html><head>
    <style>
      :root {
        --bg-color: #f0f6f9ff; /* Light blue, GAIA page light background */
        --text-color: #ff0000; /* Red for mock text */
      }
      @media (prefers-color-scheme: dark) {
        :root {
          --bg-color: #202124ff; /* Grey 900, GAIA page dark background */
        }
      }
      body {
        align-items: center;
        background-color: var(--bg-color);
        color: var(--text-color);
        display: flex;
        height: 100%;
        justify-content: center;
      }
    </style>
  </head><body><h1>Signin mocked GAIA page</h1>
  </html>
)";

std::unique_ptr<net::test_server::HttpResponse> HandleSigninURL(
    const net::test_server::HttpRequest& request) {
  // Handle the sign-in URL that ProfilePicker uses.
  if (request.relative_url.find("/signin/chrome/sync") != std::string::npos) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(kMockGaiaHtml);
    http_response->set_content_type("text/html");
    return http_response;
  }
  return nullptr;
}

}  // namespace

class ProfilePickerSigninToolbarUIPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<ProfilePickerSignInTestParam> {
 public:
  ProfilePickerSigninToolbarUIPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam().pixel_test_param) {}

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    ProfilesPixelTestBaseT<UiBrowserTest>::SetUp();
  }

  void SetUpOnMainThread() override {
    ProfilesPixelTestBaseT<UiBrowserTest>::SetUpOnMainThread();

    fake_gaia_.Initialize();

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleSigninURL));
    embedded_test_server()->StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ProfilesPixelTestBaseT<UiBrowserTest>::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kGaiaUrl,
                                    embedded_test_server()->base_url().spec());
  }

  void ShowUi(const std::string& name) override {
    gfx::ScopedAnimationDurationScaleMode disable_animation(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
    profiles::testing::WaitForPickerUrl(
        GURL("chrome://profile-picker/new-profile"));

    content::WebContents* web_contents =
        ProfilePicker::GetWebViewForTesting()->GetWebContents();

    GURL signin_url = signin::GetChromeSyncURLForDice({
        .continue_url = GaiaUrls::GetInstance()->blank_page_url(),
        .request_dark_scheme =
            static_cast<ProfilePickerView*>(ProfilePicker::GetViewForTesting())
                ->ShouldUseDarkColors(),
        .flow = signin::Flow::PROMO,
    });
    content::TestNavigationObserver navigation_observer(signin_url);
    navigation_observer.StartWatchingNewWebContents();

    ASSERT_TRUE(
        content::ExecJs(web_contents,
                        "document.querySelector('profile-picker-app')"
                        ".shadowRoot.querySelector('profile-type-choice')"
                        ".shadowRoot.querySelector('#signInButton').click()"));

    navigation_observer.Wait();
  }

  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "ProfilePickerSigninToolbarUIPixelTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    DCHECK(GetWidgetForScreenshot());
    ProfilePicker::Hide();
  }

 private:
  views::Widget* GetWidgetForScreenshot() {
    return ProfilePicker::GetViewForTesting()->GetWidget();
  }

  FakeGaia fake_gaia_;
};

IN_PROC_BROWSER_TEST_P(ProfilePickerSigninToolbarUIPixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfilePickerSigninToolbarUIPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
