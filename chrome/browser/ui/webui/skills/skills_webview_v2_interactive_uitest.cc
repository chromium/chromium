// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/skills/features.h"
#include "components/skills/internal/skills_service_impl.h"
#include "components/skills/public/skills_provider.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/base/interaction/interactive_test.h"
#include "url/gurl.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSkillsHostTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSkillsGuestId);

const InteractiveBrowserTestApi::DeepQuery kConnectionStatusQuery{
    "#connectionStatus"};
const InteractiveBrowserTestApi::DeepQuery kShowSaveToastBtnQuery{
    "#showSaveToastBtn"};
const InteractiveBrowserTestApi::DeepQuery kShowSaveAndInvokeToastBtnQuery{
    "#showSaveAndInvokeToastBtn"};
const InteractiveBrowserTestApi::DeepQuery kShowDeleteToastBtnQuery{
    "#showDeleteToastBtn"};
const InteractiveBrowserTestApi::DeepQuery kUndoStatusQuery{"#undoStatus"};
const InteractiveBrowserTestApi::DeepQuery kNavYourSkillsBtnQuery{
    "#navYourSkillsBtn"};
const InteractiveBrowserTestApi::DeepQuery kCurrentPathQuery{"#currentPath"};
const InteractiveBrowserTestApi::DeepQuery kProvidedSkillsCountQuery{
    "#providedSkillsCount"};
const InteractiveBrowserTestApi::DeepQuery kGetProvidedSkillBtnQuery{
    "#getProvidedSkillBtn"};
const InteractiveBrowserTestApi::DeepQuery kProvidedSkillDetailNameQuery{
    "#providedSkillDetailName"};
const InteractiveBrowserTestApi::DeepQuery kProvidedSkillPromptQuery{
    "#providedSkillPrompt"};

std::unique_ptr<net::test_server::HttpResponse> HandleSkillsTestClientRequest(
    const net::test_server::HttpRequest& request) {
  std::string path(request.GetURL().path());

  base::FilePath gen_dir;
  base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &gen_dir);
  base::FilePath test_client_gen_dir =
      gen_dir.AppendASCII("chrome/test/data/webui/skills/test_client");

  base::FilePath src_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir);
  base::FilePath test_client_src_dir =
      src_dir.AppendASCII("chrome/test/data/webui/skills/test_client");

  // Extract base filename if a specific asset (.js, .css, etc.) was requested.
  std::string filename = path;
  if (filename.find_last_of('/') != std::string::npos) {
    filename = filename.substr(filename.find_last_of('/') + 1);
  }

  if (!filename.empty() && filename.find('.') != std::string::npos &&
      filename != "index.html") {
    std::string content;
    base::FilePath target_file = test_client_gen_dir.AppendASCII(filename);
    if (!base::ReadFileToString(target_file, &content)) {
      target_file = test_client_src_dir.AppendASCII(filename);
      if (!base::ReadFileToString(target_file, &content)) {
        return nullptr;
      }
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    if (filename.ends_with(".js") || filename.ends_with(".ts")) {
      response->set_content_type("text/javascript");
    } else if (filename.ends_with(".css")) {
      response->set_content_type("text/css");
    } else if (filename.ends_with(".json")) {
      response->set_content_type("application/json");
    } else if (filename.ends_with(".svg")) {
      response->set_content_type("image/svg+xml");
    } else if (filename.ends_with(".png")) {
      response->set_content_type("image/png");
    }
    response->set_content(content);
    return response;
  }

  // All other paths (SPA navigation routes: /chromeskills, /chromeskills/,
  // /chromeskills/browse, /chromeskills/dialog, /index.html, /) serve
  // index.html.
  std::string html_content;
  base::FilePath html_file = test_client_gen_dir.AppendASCII("index.html");
  if (!base::ReadFileToString(html_file, &html_content)) {
    html_file = test_client_src_dir.AppendASCII("index.html");
    base::ReadFileToString(html_file, &html_content);
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(html_content);
  return response;
}

std::vector<base::test::FeatureRef> GetEnabledFeatures() {
  auto features = glic::GetDefaultEnabledGlicTestFeatures();
  features.push_back(features::kSkillsEnabled);
  features.push_back(features::kSkillsWebViewV2Enabled);
  features.push_back(features::kSkillsServiceApi);
  return features;
}

class TestSkillsProvider : public skills::SkillsProvider {
 public:
  TestSkillsProvider() = default;
  ~TestSkillsProvider() override = default;

  base::CallbackListSubscription RegisterSkillsChangedCallback(
      SkillsChangedCallback callback) override {
    return callbacks_.Add(std::move(callback));
  }

  const std::vector<std::unique_ptr<skills::Skill>>& GetSkills()
      const override {
    return skills_;
  }

  void RefreshSkills() override {}

  void AddSkill(std::unique_ptr<skills::Skill> skill) {
    skills_.push_back(std::move(skill));
    callbacks_.Notify();
  }

 private:
  base::RepeatingClosureList callbacks_;
  std::vector<std::unique_ptr<skills::Skill>> skills_;
};

}  // namespace

class SkillsWebViewV2InteractiveUITest : public InteractiveBrowserTest {
 public:
  SkillsWebViewV2InteractiveUITest()
      : glic_test_environment_(
            glic::GlicTestEnvironmentConfig{
                .force_signin_and_glic_capability = true,
                .fre_status = glic::prefs::FreStatus::kCompleted,
                .override_cookie_sync_result = true},
            GetEnabledFeatures(),
            glic::GetDefaultDisabledGlicTestFeatures()) {}
  ~SkillsWebViewV2InteractiveUITest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // Point Skills V2 Origin to the local embedded test server (without
    // trailing slash).
    std::string origin = embedded_test_server()->base_url().spec();
    if (origin.ends_with("/")) {
      origin.pop_back();
    }
    command_line->AppendSwitchASCII(switches::kSkillsV2Origin, origin);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleSkillsTestClientRequest));
    embedded_test_server()->StartAcceptingConnections();

    skills::SkillsServiceFactory::GetInstance()->SetTestingFactory(
        browser()->GetProfile(),
        base::BindRepeating(
            &SkillsWebViewV2InteractiveUITest::CreateSkillsService,
            base::Unretained(this)));
    skills::SkillsService* skills_service =
        skills::SkillsServiceFactory::GetForProfile(browser()->GetProfile());
    ASSERT_TRUE(skills_service);
    skills_service->SetServiceStatusForTesting(
        skills::SkillsService::ServiceStatus::kReady);
  }

  std::unique_ptr<KeyedService> CreateSkillsService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<skills::SkillsServiceImpl>(
        profile->GetPrefs(),
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
        IdentityManagerFactory::GetForProfile(profile), chrome::GetChannel(),
        DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  InteractiveTestApi::MultiStep CheckToastIsShowing(ToastId toast_id) {
    return PollUntil(
        [this, toast_id]() -> bool {
          auto* controller = ToastController::From(browser());
          return controller && controller->IsShowingToast() &&
                 controller->GetCurrentToastId() == toast_id;
        },
        "polling until toast is showing");
  }

 protected:
  glic::GlicTestEnvironment glic_test_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::HistogramTester histogram_tester_;
};

// Tests that the Skills V2 Webview loads the test client and completes the
// postMessage handshake successfully.
IN_PROC_BROWSER_TEST_F(SkillsWebViewV2InteractiveUITest, HandshakeAndConnect) {
  RunTestSequence(
      InstrumentTab(kSkillsHostTabId),
      NavigateWebContents(kSkillsHostTabId, GURL(chrome::kChromeUISkillsURL)),
      WaitForWebContentsReady(kSkillsHostTabId,
                              GURL(chrome::kChromeUISkillsURL)),
      InstrumentInnerWebContents(kSkillsGuestId, kSkillsHostTabId, 0),
      WaitForJsResult(kSkillsGuestId,
                      "() => window.client?.getConnected() === true"),
      WaitForJsResultAt(
          kSkillsGuestId, kConnectionStatusQuery,
          "el => el.textContent.includes('Connected to chrome://skills')",
          true));
}

// Tests triggering a Save Toast notification from the test client button.
IN_PROC_BROWSER_TEST_F(SkillsWebViewV2InteractiveUITest, ShowSaveToast) {
  RunTestSequence(
      InstrumentTab(kSkillsHostTabId),
      NavigateWebContents(kSkillsHostTabId, GURL(chrome::kChromeUISkillsURL)),
      WaitForWebContentsReady(kSkillsHostTabId,
                              GURL(chrome::kChromeUISkillsURL)),
      InstrumentInnerWebContents(kSkillsGuestId, kSkillsHostTabId, 0),
      WaitForJsResult(kSkillsGuestId,
                      "() => window.client?.getConnected() === true"),
      ExecuteJsAt(kSkillsGuestId, kShowSaveToastBtnQuery, "el => el.click()",
                  ExecuteJsMode::kFireAndForget),
      CheckToastIsShowing(ToastId::kSkillSavedWithoutInvokeButton));
}

// Tests triggering a Save and Invoke Toast notification from the test client.
IN_PROC_BROWSER_TEST_F(SkillsWebViewV2InteractiveUITest,
                       ShowSaveAndInvokeToast) {
  RunTestSequence(
      InstrumentTab(kSkillsHostTabId),
      NavigateWebContents(kSkillsHostTabId, GURL(chrome::kChromeUISkillsURL)),
      WaitForWebContentsReady(kSkillsHostTabId,
                              GURL(chrome::kChromeUISkillsURL)),
      InstrumentInnerWebContents(kSkillsGuestId, kSkillsHostTabId, 0),
      WaitForJsResult(kSkillsGuestId,
                      "() => window.client?.getConnected() === true"),
      ExecuteJsAt(kSkillsGuestId, kShowSaveAndInvokeToastBtnQuery,
                  "el => el.click()", ExecuteJsMode::kFireAndForget),
      CheckToastIsShowing(ToastId::kSkillSavedWithoutInvokeButton));
}

// Tests triggering a Delete Toast and clicking the Undo button in the toast.
IN_PROC_BROWSER_TEST_F(SkillsWebViewV2InteractiveUITest,
                       ShowDeleteToastAndUndo) {
  RunTestSequence(
      InstrumentTab(kSkillsHostTabId),
      NavigateWebContents(kSkillsHostTabId, GURL(chrome::kChromeUISkillsURL)),
      WaitForWebContentsReady(kSkillsHostTabId,
                              GURL(chrome::kChromeUISkillsURL)),
      InstrumentInnerWebContents(kSkillsGuestId, kSkillsHostTabId, 0),
      WaitForJsResult(kSkillsGuestId,
                      "() => window.client?.getConnected() === true"),
      ExecuteJsAt(kSkillsGuestId, kShowDeleteToastBtnQuery, "el => el.click()",
                  ExecuteJsMode::kFireAndForget),
      CheckToastIsShowing(ToastId::kSkillDeleted),
      PressButton(toasts::ToastView::kToastActionButton),
      WaitForJsResultAt(
          kSkillsGuestId, kUndoStatusQuery,
          "el => el.textContent.includes('Undo clicked for skillId')", true));
}

// Tests navigating between SPA routes within the guest Webview.
IN_PROC_BROWSER_TEST_F(SkillsWebViewV2InteractiveUITest,
                       NavigationAndPathUpdate) {
  RunTestSequence(
      InstrumentTab(kSkillsHostTabId),
      NavigateWebContents(kSkillsHostTabId, GURL(chrome::kChromeUISkillsURL)),
      WaitForWebContentsReady(kSkillsHostTabId,
                              GURL(chrome::kChromeUISkillsURL)),
      InstrumentInnerWebContents(kSkillsGuestId, kSkillsHostTabId, 0),
      WaitForJsResult(kSkillsGuestId,
                      "() => window.client?.getConnected() === true"),
      ExecuteJsAt(kSkillsGuestId, kNavYourSkillsBtnQuery, "el => el.click()",
                  ExecuteJsMode::kFireAndForget),
      WaitForJsResultAt(
          kSkillsGuestId, kCurrentPathQuery,
          "el => el.textContent.includes('/chromeskills/yourSkills')", true));
}

// Tests receiving provided skills from host and fetching skill details.
IN_PROC_BROWSER_TEST_F(SkillsWebViewV2InteractiveUITest,
                       ReceiveAndFetchProvidedSkills) {
  auto* skills_service =
      skills::SkillsServiceFactory::GetForProfile(browser()->GetProfile());
  auto test_provider = std::make_unique<TestSkillsProvider>();
  auto* provider_ptr = test_provider.get();
  skills_service->AddProvider(std::move(test_provider));
  provider_ptr->AddSkill(std::make_unique<skills::Skill>(
      "enterprise_01", "Enterprise Reviewer", "🏢", "Review this enterprise CL",
      "Enterprise code review skill", "Admin", GURL(),
      sync_pb::SkillSource::SKILL_SOURCE_ENTERPRISE, "Engineering"));

  RunTestSequence(
      InstrumentTab(kSkillsHostTabId),
      NavigateWebContents(kSkillsHostTabId, GURL(chrome::kChromeUISkillsURL)),
      WaitForWebContentsReady(kSkillsHostTabId,
                              GURL(chrome::kChromeUISkillsURL)),
      InstrumentInnerWebContents(kSkillsGuestId, kSkillsHostTabId, 0),
      WaitForJsResult(kSkillsGuestId,
                      "() => window.client?.getConnected() === true"),
      WaitForJsResult(
          kSkillsGuestId,
          "() => "
          "document.getElementById('providedSkillsCount')?.textContent."
          "includes('1 skills') === true"),
      ExecuteJsAt(kSkillsGuestId, kGetProvidedSkillBtnQuery, "el => el.click()",
                  ExecuteJsMode::kFireAndForget),
      WaitForJsResult(
          kSkillsGuestId,
          "() => "
          "document.getElementById('providedSkillDetailName')?.textContent."
          "includes('Enterprise Reviewer') === true"),
      WaitForJsResult(
          kSkillsGuestId,
          "() => "
          "(document.getElementById('providedSkillPrompt')?.value || '')."
          "includes('Review this enterprise CL') === true"));
}
