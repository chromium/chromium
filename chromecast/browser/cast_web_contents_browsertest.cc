// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_BROWSERTEST_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_BROWSERTEST_H_

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/cast_web_contents_observer.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "chromecast/browser/test/cast_browser_test.h"
#include "chromecast/browser/test_interfaces.test-mojom.h"
#include "chromecast/mojo/interface_bundle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::WithArgs;

namespace content {
class WebContents;
}

namespace chromecast {

namespace {

const base::FilePath::CharType kTestDataPath[] =
    FILE_PATH_LITERAL("chromecast/browser/test/data");

base::FilePath GetTestDataPath() {
  return base::FilePath(kTestDataPath);
}

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path));
  return file_path.Append(GetTestDataPath()).AppendASCII(name);
}

std::unique_ptr<net::test_server::HttpResponse> DefaultHandler(
    net::HttpStatusCode status_code,
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(status_code);
  return http_response;
}

// =============================================================================
// Mocks
// =============================================================================
class MockCastWebContentsObserver : public CastWebContentsObserver {
 public:
  MockCastWebContentsObserver() {}

  MockCastWebContentsObserver(const MockCastWebContentsObserver&) = delete;
  MockCastWebContentsObserver& operator=(const MockCastWebContentsObserver&) =
      delete;

  ~MockCastWebContentsObserver() override = default;

  MOCK_METHOD1(PageStateChanged, void(PageState page_state));
  MOCK_METHOD2(PageStopped, void(PageState page_state, int error_code));
  MOCK_METHOD2(RenderFrameCreated,
               void(int render_process_id, int render_frame_id));
  MOCK_METHOD0(ResourceLoadFailed, void());
  MOCK_METHOD1(UpdateTitle, void(const std::string& title));
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD1(CloseContents, void(content::WebContents* source));
};

class TitleChangeObserver : public CastWebContentsObserver {
 public:
  TitleChangeObserver() = default;

  TitleChangeObserver(const TitleChangeObserver&) = delete;
  TitleChangeObserver& operator=(const TitleChangeObserver&) = delete;

  ~TitleChangeObserver() override = default;

  // Spins a Runloop until the title of the page matches the |expected_title|
  // that have been set.
  void RunUntilTitleEquals(const std::string& expected_title) {
    expected_title_ = expected_title;
    // Spin the runloop until the expected conditions are met.
    if (current_title_ != expected_title_) {
      expected_title_ = expected_title;
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  // CastWebContentsObserver implementation:
  void UpdateTitle(const std::string& title) override {
    // Resumes execution of RunUntilTitleEquals() if |title| matches
    // expectations.
    current_title_ = title;
    if (!quit_closure_.is_null() && current_title_ == expected_title_) {
      DCHECK_EQ(current_title_, expected_title_);
      std::move(quit_closure_).Run();
    }
  }

 private:
  std::string current_title_;
  std::string expected_title_;

  base::OnceClosure quit_closure_;
};

class TestMessageReceiver : public blink::WebMessagePort::MessageReceiver {
 public:
  TestMessageReceiver() = default;

  TestMessageReceiver(const TestMessageReceiver&) = delete;
  TestMessageReceiver& operator=(const TestMessageReceiver&) = delete;

  ~TestMessageReceiver() override = default;

  void WaitForNextIncomingMessage(
      base::OnceCallback<void(std::string,
                              std::optional<blink::WebMessagePort>)> callback) {
    DCHECK(message_received_callback_.is_null())
        << "Only one waiting event is allowed.";
    message_received_callback_ = std::move(callback);
  }

  void SetOnPipeErrorCallback(base::OnceCallback<void()> callback) {
    on_pipe_error_callback_ = std::move(callback);
  }

 private:
  bool OnMessage(blink::WebMessagePort::Message message) override {
    std::string message_text;
    if (!base::UTF16ToUTF8(message.data.data(), message.data.size(),
                           &message_text)) {
      return false;
    }

    std::optional<blink::WebMessagePort> incoming_port = std::nullopt;
    // Only one MessagePort should be sent to here.
    if (!message.ports.empty()) {
      DCHECK(message.ports.size() == 1)
          << "Only one control port can be provided";
      incoming_port = std::make_optional<blink::WebMessagePort>(
          std::move(message.ports[0]));
    }

    if (message_received_callback_) {
      std::move(message_received_callback_)
          .Run(message_text, std::move(incoming_port));
    }
    return true;
  }

  void OnPipeError() override {
    if (on_pipe_error_callback_) {
      std::move(on_pipe_error_callback_).Run();
    }
  }

  base::OnceCallback<void(std::string,
                          std::optional<blink::WebMessagePort> incoming_port)>
      message_received_callback_;

  base::OnceCallback<void()> on_pipe_error_callback_;
};

}  // namespace

// =============================================================================
// Test class
// =============================================================================
class CastWebContentsBrowserTest : public shell::CastBrowserTest,
                                   public content::WebContentsObserver {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    CastBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures, "MojoJS");
  }

  void PreRunTestOnMainThread() override {
    // Pump startup related events.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::RunLoop().RunUntilIdle();

    metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();
    content::WebContents::CreateParams create_params(
        shell::CastBrowserProcess::GetInstance()->browser_context(), nullptr);
    web_contents_ = content::WebContents::Create(create_params);
    web_contents_->SetDelegate(&mock_wc_delegate_);

    mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
    params->is_root_window = true;
    cast_web_contents_ = std::make_unique<CastWebContentsImpl>(
        web_contents_.get(), std::move(params));
    mock_cast_wc_observer_.Observe(cast_web_contents_.get());
    title_change_observer_.Observe(cast_web_contents_.get());

    render_frames_.clear();
    content::WebContentsObserver::Observe(web_contents_.get());

    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void PostRunTestOnMainThread() override {
    cast_web_contents_.reset();
    web_contents_.reset();
  }

  // content::WebContentsObserver implementation:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) final {
    render_frames_.insert(render_frame_host);
  }

  void StartTestServer() {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

  void QuitRunLoop() {
    DCHECK(run_loop_);
    if (run_loop_->running()) {
      run_loop_->QuitWhenIdle();
    }
  }

  MockWebContentsDelegate mock_wc_delegate_;
  NiceMock<MockCastWebContentsObserver> mock_cast_wc_observer_;
  TitleChangeObserver title_change_observer_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<CastWebContentsImpl> cast_web_contents_;
  std::unique_ptr<base::RunLoop> run_loop_;

  base::flat_set<content::RenderFrameHost*> render_frames_;
};

MATCHER_P2(CheckPageState, cwc_ptr, expected_state, "") {
  if (arg != cwc_ptr) {
    return false;
  }
  return arg->page_state() == expected_state;
}

// =============================================================================
// Test cases
// =============================================================================
IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, Lifecycle) {
  // ===========================================================================
  // Test: Load a blank page successfully, verify LOADED state.
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop_->Run();

  // ===========================================================================
  // Test: Load a blank page via WebContents API, verify LOADED state.
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  web_contents_->GetController().LoadURL(GURL(url::kAboutBlankURL),
                                         content::Referrer(),
                                         ui::PAGE_TRANSITION_TYPED, "");
  run_loop_->Run();

  // ===========================================================================
  // Test: Inject an iframe, verify no events are received for the frame.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, PageStopped(_, _)).Times(0);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.src = 'about:blank';";
  ASSERT_TRUE(ExecJs(web_contents_.get(), script));

  // ===========================================================================
  // Test: Inject an iframe and navigate it to an error page. Verify no events.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, PageStopped(_, _)).Times(0);
  script = "iframe.src = 'https://www.fake-non-existent-cast-page.com';";
  ASSERT_TRUE(ExecJs(web_contents_.get(), script));

  // ===========================================================================
  // Test: Close the CastWebContents. WebContentsDelegate will be told to close
  // the page, and then after the timeout elapses CWC will enter the CLOSED
  // state and notify that the page has stopped.
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  EXPECT_CALL(mock_wc_delegate_, CloseContents(web_contents_.get()))
      .Times(AtLeast(1));
  EXPECT_CALL(mock_cast_wc_observer_, PageStopped(PageState::CLOSED, net::OK))
      .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  cast_web_contents_->ClosePage();
  run_loop_->Run();

  // ===========================================================================
  // Test: Destroy the underlying WebContents. Verify DESTROYED state.
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::DESTROYED))
      .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  web_contents_.reset();
  run_loop_->Run();
  cast_web_contents_.reset();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, WebContentsDestroyed) {
  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop_->Run();

  // ===========================================================================
  // Test: Destroy the WebContents. Verify PageStopped(DESTROYED, net::OK).
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  EXPECT_CALL(mock_cast_wc_observer_,
              PageStopped(PageState::DESTROYED, net::OK))
      .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  web_contents_.reset();
  run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorPageCrash) {
  // ===========================================================================
  // Test: If the page's main render process crashes, enter ERROR state.
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop_->Run();

  run_loop_ = std::make_unique<base::RunLoop>();
  EXPECT_CALL(mock_cast_wc_observer_,
              PageStopped(PageState::ERROR, net::ERR_UNEXPECTED))
      .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  CrashTab(web_contents_.get());
  run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorLocalFileMissing) {
  // ===========================================================================
  // Test: Loading a page with an HTTP error should enter ERROR state.
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStopped(PageState::ERROR, _))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  base::FilePath path = GetTestDataFilePath("this_file_does_not_exist.html");
  cast_web_contents_->LoadUrl(content::GetFileUrlWithQuery(path, ""));
  run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorLoadFailSubFrames) {
  // ===========================================================================
  // Test: Ignore load errors in sub-frames.
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop_->Run();

  // Create a sub-frame.
  EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, PageStopped(_, _)).Times(0);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.src = 'about:blank';";
  ASSERT_TRUE(ExecJs(web_contents_.get(), script));

  ASSERT_EQ(2, (int)render_frames_.size());
  auto it =
      base::ranges::find(render_frames_, web_contents_->GetPrimaryMainFrame(),
                         &content::RenderFrameHost::GetParent);
  ASSERT_NE(render_frames_.end(), it);
  content::RenderFrameHost* sub_frame = *it;
  ASSERT_NE(nullptr, sub_frame);
  cast_web_contents_->DidFailLoad(sub_frame, sub_frame->GetLastCommittedURL(),
                                  net::ERR_FAILED);

  // ===========================================================================
  // Test: Ignore main frame load failures with net::ERR_ABORTED.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, PageStopped(_, _)).Times(0);
  cast_web_contents_->DidFailLoad(
      web_contents_->GetPrimaryMainFrame(),
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      net::ERR_ABORTED);

  // ===========================================================================
  // Test: If main frame fails to load, page should enter ERROR state.
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  EXPECT_CALL(mock_cast_wc_observer_,
              PageStopped(PageState::ERROR, net::ERR_FAILED))
      .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  cast_web_contents_->DidFailLoad(
      web_contents_->GetPrimaryMainFrame(),
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      net::ERR_FAILED);
  run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorHttp4XX) {
  // ===========================================================================
  // Test: If a server responds with an HTTP 4XX error, page should enter ERROR
  // state.
  // ===========================================================================
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&DefaultHandler, net::HTTP_NOT_FOUND));
  StartTestServer();

  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        PageStopped(PageState::ERROR, net::ERR_HTTP_RESPONSE_CODE_FAILURE))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  cast_web_contents_->LoadUrl(embedded_test_server()->GetURL("/dummy.html"));
  run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorLoadFailed) {
  // ===========================================================================
  // Test: When main frame load fails, enter ERROR state. This test simulates a
  // load error by intercepting the URL request and failing it with an arbitrary
  // error code.
  // ===========================================================================
  base::FilePath path = GetTestDataFilePath("dummy.html");
  GURL gurl = content::GetFileUrlWithQuery(path, "");
  content::URLLoaderInterceptor url_interceptor(base::BindRepeating(
      [](const GURL& url,
         content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url != url) {
          return false;
        }
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_ADDRESS_UNREACHABLE;
        params->client->OnComplete(status);
        return true;
      },
      gurl));

  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_,
                PageStopped(PageState::ERROR, net::ERR_ADDRESS_UNREACHABLE))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  cast_web_contents_->LoadUrl(gurl);
  run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, LoadCanceledByApp) {
  // ===========================================================================
  // Test: When the app calls window.stop(), the page should not enter the ERROR
  // state. Instead, we treat it as LOADED. This is a historical behavior for
  // some apps which intentionally stop the page and reload content.
  // ===========================================================================
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();

  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  cast_web_contents_->LoadUrl(
      embedded_test_server()->GetURL("/load_cancel.html"));
  run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, LocationRedirectLifecycle) {
  // ===========================================================================
  // Test: When the app redirects to another url via window.location. Another
  // navigation will be committed. LOADING -> LOADED -> LOADING -> LOADED state
  // trasition is expected.
  // ===========================================================================
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();

  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  cast_web_contents_->LoadUrl(
      embedded_test_server()->GetURL("/location_redirect.html"));
  run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, NotifyMissingResource) {
  // ===========================================================================
  // Test: Loading a page with a missing resource should notify observers.
  // ===========================================================================
  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }
  EXPECT_CALL(mock_cast_wc_observer_, ResourceLoadFailed());

  base::FilePath path = GetTestDataFilePath("missing_resource.html");
  cast_web_contents_->LoadUrl(content::GetFileUrlWithQuery(path, ""));
  run_loop_->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ExecuteJavaScriptOnLoad) {
  // ===========================================================================
  // Test: Injecting script to change title should work.
  // ===========================================================================
  const std::string kExpectedTitle = "hello";
  const std::string kOriginalTitle =
      "Welcome to Stan the Offline Dino's Homepage";

  // The script should be able to run before HTML <script> tag starts running.
  // The original title will be loaded first and then the injected script. Other
  // scripts must run after the injected script.
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kExpectedTitle));
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kOriginalTitle));
  constexpr uint64_t kBindingsId = 1234;

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(kBindingsId,
                                              "stashed_title = 'hello';");

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kExpectedTitle);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       ExecuteJavaScriptUpdatedOnLoad) {
  // ===========================================================================
  // Test: Verify that this script replaces the previous script with same
  // binding id, as opposed to being injected alongside it. (The latter would
  // result in the title being "helloclobber").
  // ===========================================================================
  const std::string kReplaceTitle = "clobber";
  const std::string kOriginalTitle =
      "Welcome to Stan the Offline Dino's Homepage";

  // The script should be able to run before HTML <script> tag starts running.
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kReplaceTitle));
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kOriginalTitle));

  constexpr uint64_t kBindingsId = 1234;

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(kBindingsId,
                                              "stashed_title = 'hello';");

  cast_web_contents_->AddBeforeLoadJavaScript(
      kBindingsId, "stashed_title = document.title + 'clobber';");

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kReplaceTitle);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       ExecuteJavaScriptOnLoadOrdered) {
  // ===========================================================================
  // Test: Verifies that bindings are injected in order by producing a
  // cumulative, non-commutative result.
  // ===========================================================================
  const std::string kExpectedTitle = "hello there";
  const std::string kOriginalTitle =
      "Welcome to Stan the Offline Dino's Homepage";
  constexpr int64_t kBindingsId1 = 1234;
  constexpr int64_t kBindingsId2 = 5678;

  // The script should be able to run before HTML <script> tag starts running.
  // The original title will be loaded first and then the injected script. Other
  // scripts must run after the injected script.
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kExpectedTitle));
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kOriginalTitle));

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(kBindingsId1,
                                              "stashed_title = 'hello';");

  cast_web_contents_->AddBeforeLoadJavaScript(kBindingsId2,
                                              "stashed_title += ' there';");

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kExpectedTitle);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       ExecuteJavaScriptOnLoadEarlyAndLateRegistrations) {
  // ===========================================================================
  // Test: Tests that we can inject scripts before and after RenderFrame
  // creation.
  // ===========================================================================
  const std::string kExpectedTitle1 = "foo";
  const std::string kExpectedTitle2 = "foo bar";
  const std::string kOriginalTitle =
      "Welcome to Stan the Offline Dino's Homepage";
  constexpr int64_t kBindingsId1 = 1234;
  constexpr int64_t kBindingsId2 = 5678;

  // The script should be able to run before HTML <script> tag starts running.
  // The original title will be loaded first and then the injected script. Other
  // scripts must run after the injected script.
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kExpectedTitle2));
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kExpectedTitle1));
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kOriginalTitle)).Times(2);

  GURL gurl = content::GetFileUrlWithQuery(
      GetTestDataFilePath("dynamic_title.html"), "");

  cast_web_contents_->AddBeforeLoadJavaScript(kBindingsId1,
                                              "stashed_title = 'foo';");
  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kExpectedTitle1);

  // Inject bindings after RenderFrameCreation
  cast_web_contents_->AddBeforeLoadJavaScript(kBindingsId2,
                                              "stashed_title += ' bar';");

  // Navigate away to clean the state.
  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));

  // Navigate back and see if both scripts are working.
  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kExpectedTitle2);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, PostMessageToMainFrame) {
  // ===========================================================================
  // Test: Tests that we can trigger onmessage event on a web page. This test
  // would post a message to the test page to redirect it to |title1.html|.
  // ===========================================================================
  constexpr char kOriginalTitle[] = "postmessage";
  constexpr char kPage1Path[] = "title1.html";
  constexpr char kPage1Title[] = "title 1";

  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kPage1Title));
  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kOriginalTitle));

  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();
  GURL gurl = embedded_test_server()->GetURL("/window_post_message.html");

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kOriginalTitle);

  cast_web_contents_->PostMessageToMainFrame(
      gurl.DeprecatedGetOriginAsURL().spec(), kPage1Path,
      std::vector<blink::WebMessagePort>());
  title_change_observer_.RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, PostMessagePassMessagePort) {
  // ===========================================================================
  // Test: Send a MessagePort to the page, then perform bidirectional messaging
  // through the port.
  // ===========================================================================
  constexpr char kOriginalTitle[] = "messageport";
  constexpr char kHelloMsg[] = "hi";
  constexpr char16_t kPingMsg[] = u"ping";

  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kOriginalTitle));

  // Load test page.
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();
  GURL gurl = embedded_test_server()->GetURL("/message_port.html");
  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kOriginalTitle);

  auto message_pipe = blink::WebMessagePort::CreatePair();
  auto platform_port = std::move(message_pipe.first);
  auto page_port = std::move(message_pipe.second);

  TestMessageReceiver message_receiver;
  platform_port.SetReceiver(&message_receiver,
                            base::SingleThreadTaskRunner::GetCurrentDefault());

  // Make sure we could send a MessagePort (ScopedMessagePipeHandle) to the
  // page.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auto received_message_callback = base::BindOnce(
        [](base::OnceClosure loop_quit_closure, std::string port_msg,
           std::optional<blink::WebMessagePort> incoming_port) {
          EXPECT_EQ("got_port", port_msg);
          std::move(loop_quit_closure).Run();
        },
        std::move(quit_closure));
    message_receiver.WaitForNextIncomingMessage(
        std::move(received_message_callback));
    std::vector<blink::WebMessagePort> message_ports;
    message_ports.push_back(std::move(page_port));
    cast_web_contents_->PostMessageToMainFrame(
        gurl.DeprecatedGetOriginAsURL().spec(), kHelloMsg,
        std::move(message_ports));
    run_loop.Run();
  }
  // Test whether we could receive the right response from the page after we
  // send messages through |platform_port|.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auto received_message_callback = base::BindOnce(
        [](base::OnceClosure loop_quit_closure, std::string port_msg,
           std::optional<blink::WebMessagePort> incoming_port) {
          EXPECT_EQ("ack ping", port_msg);
          std::move(loop_quit_closure).Run();
        },
        std::move(quit_closure));
    message_receiver.WaitForNextIncomingMessage(
        std::move(received_message_callback));
    platform_port.PostMessage(blink::WebMessagePort::Message(kPingMsg));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       PostMessageMessagePortDisconnected) {
  // ===========================================================================
  // Test: Send a MessagePort to the page, then perform bidirectional messaging
  // through the port. Make sure mojo counterpart pipe handle could receive the
  // MessagePort disconnection event.
  // ===========================================================================
  constexpr char kOriginalTitle[] = "messageport";
  constexpr char kHelloMsg[] = "hi";

  EXPECT_CALL(mock_cast_wc_observer_, UpdateTitle(kOriginalTitle));
  // Load test page.
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();
  GURL gurl = embedded_test_server()->GetURL("/message_port.html");
  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kOriginalTitle);

  auto message_pipe = blink::WebMessagePort::CreatePair();
  auto platform_port = std::move(message_pipe.first);
  auto page_port = std::move(message_pipe.second);

  // Bind platform side port
  TestMessageReceiver message_receiver;
  platform_port.SetReceiver(&message_receiver,
                            base::SingleThreadTaskRunner::GetCurrentDefault());

  // Make sure we could post a MessagePort (ScopedMessagePipeHandle) to
  // the page.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auto received_message_callback = base::BindOnce(
        [](base::OnceClosure loop_quit_closure, std::string port_msg,
           std::optional<blink::WebMessagePort> incoming_port) {
          EXPECT_EQ("got_port", port_msg);
          std::move(loop_quit_closure).Run();
        },
        std::move(quit_closure));
    message_receiver.WaitForNextIncomingMessage(
        std::move(received_message_callback));
    std::vector<blink::WebMessagePort> message_ports;
    message_ports.push_back(std::move(page_port));
    cast_web_contents_->PostMessageToMainFrame(
        gurl.DeprecatedGetOriginAsURL().spec(), kHelloMsg,
        std::move(message_ports));
    run_loop.Run();
  }
  // Navigating off-page should tear down the MessageChannel, native side
  // should be able to receive disconnected event.
  {
    base::RunLoop run_loop;
    message_receiver.SetOnPipeErrorCallback(base::BindOnce(
        [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
        run_loop.QuitClosure()));
    cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ExecuteJavaScript) {
  // Start test server for hosting test HTML pages.
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();

  // ===========================================================================
  // Test: Set a value using ExecuteJavaScript with empty callback, and then use
  // ExecuteJavaScript with callback to retrieve that value.
  // ===========================================================================
  constexpr char kSoyMilkJsonStringLiteral[] = "\"SoyMilk\"";
  constexpr char16_t kSoyMilkJsonStringLiteral16[] = u"\"SoyMilk\"";

  // Load page with title "hello":
  GURL gurl{embedded_test_server()->GetURL("/title1.html")};
  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }
  cast_web_contents_->LoadUrl(gurl);
  run_loop_->Run();

  // Execute with empty callback.
  cast_web_contents_->ExecuteJavaScript(
      base::StrCat({u"const the_var = ", kSoyMilkJsonStringLiteral16, u";"}),
      base::DoNothing());

  // Execute a script snippet to return the variable's value.
  base::RunLoop run_loop2;
  cast_web_contents_->ExecuteJavaScript(
      u"the_var;", base::BindLambdaForTesting([&](base::Value result_value) {
        std::string result_json;
        ASSERT_TRUE(base::JSONWriter::Write(result_value, &result_json));
        EXPECT_EQ(result_json, kSoyMilkJsonStringLiteral);
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

// Mock class used by the following test case.
class MockApiBindings : public mojom::ApiBindings {
 public:
  MockApiBindings() = default;
  ~MockApiBindings() override = default;

  mojo::PendingRemote<mojom::ApiBindings> CreateRemote() {
    DCHECK(!receiver_.is_bound());

    mojo::PendingRemote<mojom::ApiBindings> pending_remote =
        receiver_.BindNewPipeAndPassRemote();

    return pending_remote;
  }

  // mojom::ApiBindings implementation:
  MOCK_METHOD(void, GetAll, (GetAllCallback), (override));
  MOCK_METHOD(void,
              Connect,
              (const std::string&, blink::MessagePortDescriptor),
              (override));

 private:
  mojo::Receiver<mojom::ApiBindings> receiver_{this};
};

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       InjectBindingsFromApiBindingsRemote) {
  // Start test server for hosting test HTML pages.
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();

  // ===========================================================================
  // Test: Inject a set of scripts to eval an result. Retrieve that value and
  // match against the right answer.
  // ===========================================================================
  MockApiBindings mock_api_bindings;
  EXPECT_CALL(mock_api_bindings, GetAll(_))
      .Times(1)
      .WillOnce(
          WithArgs<0>(Invoke([](MockApiBindings::GetAllCallback callback) {
            std::vector<chromecast::mojom::ApiBindingPtr> bindings_vector;
            bindings_vector.emplace_back(
                chromecast::mojom::ApiBinding::New("let res = 0;"));
            bindings_vector.emplace_back(
                chromecast::mojom::ApiBinding::New("res += 1;"));
            bindings_vector.emplace_back(
                chromecast::mojom::ApiBinding::New("res += 2;"));
            bindings_vector.emplace_back(
                chromecast::mojom::ApiBinding::New("res += 3;"));
            std::move(callback).Run(std::move(bindings_vector));
          })));

  // Binds mocked |mojom::ApiBindings|.
  cast_web_contents_->ConnectToBindingsService(
      mock_api_bindings.CreateRemote());

  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  // Loads a blank page.
  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop_->Run();

  // Evaluates the value of |res|.
  EXPECT_EQ(6, content::EvalJs(cast_web_contents_->web_contents(), "res;"));
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest,
                       StopPageInCaseOfEmptyBindingsReceived) {
  // Start test server for hosting test HTML pages.
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();

  // ===========================================================================
  // Test: Sending empty set of bindings should result in error page state.
  // ===========================================================================
  MockApiBindings mock_api_bindings;
  EXPECT_CALL(mock_api_bindings, GetAll(_))
      .Times(1)
      .WillOnce(
          WithArgs<0>(Invoke([](MockApiBindings::GetAllCallback callback) {
            std::vector<chromecast::mojom::ApiBindingPtr> bindings_vector;
            std::move(callback).Run(std::move(bindings_vector));
          })));

  // Binds mocked |mojom::ApiBindings|.
  cast_web_contents_->ConnectToBindingsService(
      mock_api_bindings.CreateRemote());

  run_loop_ = std::make_unique<base::RunLoop>();
  {
    InSequence seq;
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADING));
    EXPECT_CALL(mock_cast_wc_observer_, PageStateChanged(PageState::LOADED))
        .WillOnce(InvokeWithoutArgs([&]() { QuitRunLoop(); }));
  }

  EXPECT_CALL(mock_cast_wc_observer_,
              PageStopped(PageState::ERROR, net::ERR_UNEXPECTED));

  // Loads a blank page.
  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop_->Run();
}

// Helper for the test below. This exposes two interfaces, TestAdder and
// TestDoubler.
class TestInterfaceProvider : public mojom::TestAdder,
                              public mojom::TestDoubler {
 public:
  TestInterfaceProvider() = default;
  ~TestInterfaceProvider() override = default;

  size_t num_adders() const { return adders_.size(); }
  size_t num_doublers() const { return doublers_.size(); }

  base::RepeatingCallback<void(mojo::PendingReceiver<mojom::TestAdder>)>
  MakeAdderBinder() {
    return base::BindLambdaForTesting(
        [this](mojo::PendingReceiver<mojom::TestAdder> receiver) {
          adders_.Add(this, std::move(receiver));
          OnRequestHandled();
        });
  }

  base::RepeatingCallback<void(mojo::PendingReceiver<mojom::TestDoubler>)>
  MakeDoublerBinder() {
    return base::BindLambdaForTesting(
        [this](mojo::PendingReceiver<mojom::TestDoubler> receiver) {
          doublers_.Add(this, std::move(receiver));
          OnRequestHandled();
        });
  }

  // Waits for some number of new interface binding requests to be dispatched
  // and then invokes `callback`.
  void WaitForRequests(size_t n, base::OnceClosure callback) {
    wait_callback_ = std::move(callback);
    num_requests_to_wait_for_ = n;
  }

  // mojom::TestAdder:
  void Add(int32_t a, int32_t b, AddCallback callback) override {
    std::move(callback).Run(a + b);
  }

  // mojom::TestDouble:
  void Double(int32_t x, DoubleCallback callback) override {
    std::move(callback).Run(x * 2);
  }

 private:
  void OnRequestHandled() {
    if (num_requests_to_wait_for_ == 0) {
      return;
    }
    DCHECK(wait_callback_);
    if (--num_requests_to_wait_for_ == 0) {
      std::move(wait_callback_).Run();
    }
  }

  mojo::ReceiverSet<mojom::TestAdder> adders_;
  mojo::ReceiverSet<mojom::TestDoubler> doublers_;
  size_t num_requests_to_wait_for_ = 0;
  base::OnceClosure wait_callback_;
};

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, InterfaceBinding) {
  // This test verifies that interfaces registered with the CastWebContents --
  // either via its binder_registry() or its RegisterInterfaceProvider() API --
  // are reachable from render frames using either the deprecated
  // InterfaceProvider API (which results in an OnInterfaceRequestFromFrame call
  // on the WebContents) or the newer BrowserInterfaceBroker API which is used
  // in most other places (including from Mojo JS).
  TestInterfaceProvider provider;
  InterfaceBundle bundle_;
  bundle_.AddBinder(provider.MakeAdderBinder());
  bundle_.AddBinder(provider.MakeDoublerBinder());
  cast_web_contents_->SetInterfacesForRenderer(bundle_.CreateRemote());

  // First verify that both interfaces are reachable using the deprecated
  // WebContents path, which is triggered only by renderer-side use of
  // RenderFrame::GetRemoteInterfaces(). Since poking renderer state in browser
  // tests is challenging, we simply simulate the resulting WebContentsObbserver
  // calls here instead and verify end-to-end connection for each interface.
  mojo::Remote<mojom::TestAdder> adder;
  mojo::GenericPendingReceiver adder_receiver(
      adder.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(cast_web_contents_->TryBindReceiver(adder_receiver));

  mojo::Remote<mojom::TestDoubler> doubler;
  mojo::GenericPendingReceiver doubler_receiver(
      doubler.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(cast_web_contents_->TryBindReceiver(doubler_receiver));

  base::RunLoop add_loop;
  adder->Add(37, 5, base::BindLambdaForTesting([&](int32_t result) {
               EXPECT_EQ(42, result);
               add_loop.Quit();
             }));
  add_loop.Run();

  base::RunLoop double_loop;
  doubler->Double(21, base::BindLambdaForTesting([&](int32_t result) {
                    EXPECT_EQ(42, result);
                    double_loop.Quit();
                  }));
  double_loop.Run();

  EXPECT_EQ(1u, provider.num_adders());
  EXPECT_EQ(1u, provider.num_doublers());

  // Now verify that the same interfaces are also reachable at the same binders
  // when going through the newer BrowserInterfaceBroker path. For simplicity
  // the test JS here does not have access to bindings and so does not make
  // calls on the interfaces. It is however totally sufficient for us to verify
  // that the page's requests result in new receivers being bound inside
  // TestInterfaceProvider.
  base::RunLoop loop;
  provider.WaitForRequests(2, loop.QuitClosure());
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();
  const GURL kUrl{embedded_test_server()->GetURL("/interface_binding.html")};
  cast_web_contents_->LoadUrl(kUrl);
  loop.Run();

  EXPECT_EQ(2u, provider.num_adders());
  EXPECT_EQ(2u, provider.num_doublers());
}

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_BROWSERTEST_H_
