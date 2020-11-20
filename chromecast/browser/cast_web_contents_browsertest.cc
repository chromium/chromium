// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_BROWSERTEST_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_BROWSERTEST_H_

#include <algorithm>
#include <memory>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/test_interfaces.test-mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
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
using ::testing::Eq;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Property;

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
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
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
class MockCastWebContentsDelegate
    : public base::SupportsWeakPtr<MockCastWebContentsDelegate>,
      public CastWebContents::Delegate {
 public:
  MockCastWebContentsDelegate() {}
  ~MockCastWebContentsDelegate() override = default;

  MOCK_METHOD2(InnerContentsCreated,
               void(CastWebContents* inner_contents,
                    CastWebContents* outer_contents));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastWebContentsDelegate);
};

class MockCastWebContentsObserver : public CastWebContents::Observer {
 public:
  MockCastWebContentsObserver() {}
  ~MockCastWebContentsObserver() override = default;

  MOCK_METHOD1(OnPageStateChanged, void(CastWebContents* cast_web_contents));
  MOCK_METHOD2(OnPageStopped,
               void(CastWebContents* cast_web_contents, int error_code));
  MOCK_METHOD4(
      RenderFrameCreated,
      void(int render_process_id,
           int render_frame_id,
           service_manager::InterfaceProvider* frame_interfaces,
           blink::AssociatedInterfaceProvider* frame_associated_interfaces));
  MOCK_METHOD1(ResourceLoadFailed, void(CastWebContents* cast_web_contents));
  MOCK_METHOD1(UpdateTitle, void(const base::string16& title));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastWebContentsObserver);
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD1(CloseContents, void(content::WebContents* source));
};

class TitleChangeObserver : public CastWebContents::Observer {
 public:
  TitleChangeObserver() = default;
  ~TitleChangeObserver() override = default;

  // Spins a Runloop until the title of the page matches the |expected_title|
  // that have been set.
  void RunUntilTitleEquals(base::StringPiece expected_title) {
    expected_title_ = expected_title.as_string();
    // Spin the runloop until the expected conditions are met.
    if (current_title_ != expected_title_) {
      expected_title_ = expected_title.as_string();
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  // CastWebContents::Observer implementation:
  void UpdateTitle(const base::string16& title) override {
    // Resumes execution of RunUntilTitleEquals() if |title| matches
    // expectations.
    std::string title_utf8 = base::UTF16ToUTF8(title);
    current_title_ = title_utf8;
    if (!quit_closure_.is_null() && current_title_ == expected_title_) {
      DCHECK_EQ(current_title_, expected_title_);
      std::move(quit_closure_).Run();
    }
  }

 private:
  std::string current_title_;
  std::string expected_title_;

  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TitleChangeObserver);
};

class TestMessageReceiver : public blink::WebMessagePort::MessageReceiver {
 public:
  TestMessageReceiver() = default;
  ~TestMessageReceiver() override = default;

  void WaitForNextIncomingMessage(
      base::OnceCallback<
          void(std::string, base::Optional<blink::WebMessagePort>)> callback) {
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

    base::Optional<blink::WebMessagePort> incoming_port = base::nullopt;
    // Only one MessagePort should be sent to here.
    if (!message.ports.empty()) {
      DCHECK(message.ports.size() == 1)
          << "Only one control port can be provided";
      incoming_port = base::make_optional<blink::WebMessagePort>(
          std::move(message.ports[0]));
    }

    if (message_received_callback_) {
      std::move(message_received_callback_)
          .Run(message_text, std::move(incoming_port));
    }
    return true;
  }

  void OnPipeError() override {
    if (on_pipe_error_callback_)
      std::move(on_pipe_error_callback_).Run();
  }

  base::OnceCallback<void(std::string,
                          base::Optional<blink::WebMessagePort> incoming_port)>
      message_received_callback_;

  base::OnceCallback<void()> on_pipe_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageReceiver);
};

}  // namespace

// =============================================================================
// Test class
// =============================================================================
class CastWebContentsBrowserTest : public content::BrowserTestBase,
                                   public content::WebContentsObserver {
 protected:
  CastWebContentsBrowserTest() = default;
  ~CastWebContentsBrowserTest() override = default;

  void SetUp() final {
    SetUpCommandLine(base::CommandLine::ForCurrentProcess());
    BrowserTestBase::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kTestType, "browser");
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

    CastWebContents::InitParams init_params;
    init_params.delegate = mock_cast_wc_delegate_.AsWeakPtr();
    init_params.is_root_window = true;

    cast_web_contents_ =
        std::make_unique<CastWebContentsImpl>(web_contents_.get(), init_params);
    mock_cast_wc_observer_.Observe(cast_web_contents_.get());
    title_change_observer_.Observe(cast_web_contents_.get());

    render_frames_.clear();
    content::WebContentsObserver::Observe(web_contents_.get());
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

  MockWebContentsDelegate mock_wc_delegate_;
  MockCastWebContentsDelegate mock_cast_wc_delegate_;
  NiceMock<MockCastWebContentsObserver> mock_cast_wc_observer_;
  TitleChangeObserver title_change_observer_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<CastWebContentsImpl> cast_web_contents_;

  base::flat_set<content::RenderFrameHost*> render_frames_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastWebContentsBrowserTest);
};

MATCHER_P2(CheckPageState, cwc_ptr, expected_state, "") {
  if (arg != cwc_ptr)
    return false;
  return arg->page_state() == expected_state;
}

// =============================================================================
// Test cases
// =============================================================================
IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, Lifecycle) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: Load a blank page successfully, verify LOADED state.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop->Run();

  // ===========================================================================
  // Test: Load a blank page via WebContents API, verify LOADED state.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  run_loop = std::make_unique<base::RunLoop>();
  web_contents_->GetController().LoadURL(GURL(url::kAboutBlankURL),
                                         content::Referrer(),
                                         ui::PAGE_TRANSITION_TYPED, "");
  run_loop->Run();

  // ===========================================================================
  // Test: Inject an iframe, verify no events are received for the frame.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStopped(_, _)).Times(0);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.src = 'about:blank';";
  ASSERT_TRUE(ExecJs(web_contents_.get(), script));

  // ===========================================================================
  // Test: Inject an iframe and navigate it to an error page. Verify no events.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStopped(_, _)).Times(0);
  script = "iframe.src = 'https://www.fake-non-existent-cast-page.com';";
  ASSERT_TRUE(ExecJs(web_contents_.get(), script));

  // ===========================================================================
  // Test: Close the CastWebContents. WebContentsDelegate will be told to close
  // the page, and then after the timeout elapses CWC will enter the CLOSED
  // state and notify that the page has stopped.
  // ===========================================================================
  EXPECT_CALL(mock_wc_delegate_, CloseContents(web_contents_.get()))
      .Times(AtLeast(1));
  EXPECT_CALL(mock_cast_wc_observer_,
              OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                           CastWebContents::PageState::CLOSED),
                            net::OK))
      .WillOnce(InvokeWithoutArgs(quit_closure));
  run_loop = std::make_unique<base::RunLoop>();
  cast_web_contents_->ClosePage();
  run_loop->Run();

  // ===========================================================================
  // Test: Destroy the underlying WebContents. Verify DESTROYED state.
  // ===========================================================================
  EXPECT_CALL(
      mock_cast_wc_observer_,
      OnPageStateChanged(CheckPageState(
          cast_web_contents_.get(), CastWebContents::PageState::DESTROYED)));
  web_contents_.reset();
  cast_web_contents_.reset();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, WebContentsDestroyed) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop->Run();

  // ===========================================================================
  // Test: Destroy the WebContents. Verify OnPageStopped(DESTROYED, net::OK).
  // ===========================================================================
  EXPECT_CALL(
      mock_cast_wc_observer_,
      OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                   CastWebContents::PageState::DESTROYED),
                    net::OK));
  web_contents_.reset();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorPageCrash) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: If the page's main render process crashes, enter ERROR state.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop->Run();

  EXPECT_CALL(mock_cast_wc_observer_,
              OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                           CastWebContents::PageState::ERROR),
                            net::ERR_UNEXPECTED));
  CrashTab(web_contents_.get());
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorLocalFileMissing) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: Loading a page with an HTTP error should enter ERROR state.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(mock_cast_wc_observer_,
                OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                             CastWebContents::PageState::ERROR),
                              _))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  base::FilePath path = GetTestDataFilePath("this_file_does_not_exist.html");
  cast_web_contents_->LoadUrl(content::GetFileUrlWithQuery(path, ""));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorLoadFailSubFrames) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: Ignore load errors in sub-frames.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(GURL(url::kAboutBlankURL));
  run_loop->Run();

  // Create a sub-frame.
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStopped(_, _)).Times(0);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "document.body.appendChild(iframe);"
      "iframe.src = 'about:blank';";
  ASSERT_TRUE(ExecJs(web_contents_.get(), script));

  ASSERT_EQ(2, (int)render_frames_.size());
  auto it =
      std::find_if(render_frames_.begin(), render_frames_.end(),
                   [this](content::RenderFrameHost* frame) {
                     return frame->GetParent() == web_contents_->GetMainFrame();
                   });
  ASSERT_NE(render_frames_.end(), it);
  content::RenderFrameHost* sub_frame = *it;
  ASSERT_NE(nullptr, sub_frame);
  cast_web_contents_->DidFailLoad(sub_frame, sub_frame->GetLastCommittedURL(),
                                  net::ERR_FAILED);

  // ===========================================================================
  // Test: Ignore main frame load failures with net::ERR_ABORTED.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStateChanged(_)).Times(0);
  EXPECT_CALL(mock_cast_wc_observer_, OnPageStopped(_, _)).Times(0);
  cast_web_contents_->DidFailLoad(
      web_contents_->GetMainFrame(),
      web_contents_->GetMainFrame()->GetLastCommittedURL(), net::ERR_ABORTED);

  // ===========================================================================
  // Test: If main frame fails to load, page should enter ERROR state.
  // ===========================================================================
  EXPECT_CALL(mock_cast_wc_observer_,
              OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                           CastWebContents::PageState::ERROR),
                            net::ERR_FAILED));
  cast_web_contents_->DidFailLoad(
      web_contents_->GetMainFrame(),
      web_contents_->GetMainFrame()->GetLastCommittedURL(), net::ERR_FAILED);
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorHttp4XX) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: If a server responds with an HTTP 4XX error, page should enter ERROR
  // state.
  // ===========================================================================
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&DefaultHandler, net::HTTP_NOT_FOUND));
  StartTestServer();

  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(mock_cast_wc_observer_,
                OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                             CastWebContents::PageState::ERROR),
                              net::ERR_HTTP_RESPONSE_CODE_FAILURE))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(embedded_test_server()->GetURL("/dummy.html"));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, ErrorLoadFailed) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

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
        if (params->url_request.url != url)
          return false;
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_ADDRESS_UNREACHABLE;
        params->client->OnComplete(status);
        return true;
      },
      gurl));

  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(mock_cast_wc_observer_,
                OnPageStopped(CheckPageState(cast_web_contents_.get(),
                                             CastWebContents::PageState::ERROR),
                              net::ERR_ADDRESS_UNREACHABLE))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(gurl);
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, LoadCanceledByApp) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: When the app calls window.stop(), the page should not enter the ERROR
  // state. Instead, we treat it as LOADED. This is a historical behavior for
  // some apps which intentionally stop the page and reload content.
  // ===========================================================================
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();

  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(
      embedded_test_server()->GetURL("/load_cancel.html"));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, LocationRedirectLifecycle) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: When the app redirects to another url via window.location. Another
  // navigation will be committed. LOADING -> LOADED -> LOADING -> LOADED state
  // trasition is expected.
  // ===========================================================================
  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();

  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }

  cast_web_contents_->LoadUrl(
      embedded_test_server()->GetURL("/location_redirect.html"));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, NotifyMissingResource) {
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: Loading a page with a missing resource should notify observers.
  // ===========================================================================
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }
  EXPECT_CALL(mock_cast_wc_observer_,
              ResourceLoadFailed(cast_web_contents_.get()));

  base::FilePath path = GetTestDataFilePath("missing_resource.html");
  cast_web_contents_->LoadUrl(content::GetFileUrlWithQuery(path, ""));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(CastWebContentsBrowserTest, PostMessageToMainFrame) {
  // ===========================================================================
  // Test: Tests that we can trigger onmessage event on a web page. This test
  // would post a message to the test page to redirect it to |title1.html|.
  // ===========================================================================
  constexpr char kOriginalTitle[] = "postmessage";
  constexpr char kPage1Path[] = "title1.html";
  constexpr char kPage1Title[] = "title 1";

  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kPage1Title)));
  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)));

  embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataPath());
  StartTestServer();
  GURL gurl = embedded_test_server()->GetURL("/window_post_message.html");

  cast_web_contents_->LoadUrl(gurl);
  title_change_observer_.RunUntilTitleEquals(kOriginalTitle);

  cast_web_contents_->PostMessageToMainFrame(
      gurl.GetOrigin().spec(), std::string(kPage1Path),
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
  constexpr char kPingMsg[] = "ping";

  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)));

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
                            base::ThreadTaskRunnerHandle::Get());

  // Make sure we could send a MessagePort (ScopedMessagePipeHandle) to the
  // page.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auto received_message_callback = base::BindOnce(
        [](base::OnceClosure loop_quit_closure, std::string port_msg,
           base::Optional<blink::WebMessagePort> incoming_port) {
          EXPECT_EQ("got_port", port_msg);
          std::move(loop_quit_closure).Run();
        },
        std::move(quit_closure));
    message_receiver.WaitForNextIncomingMessage(
        std::move(received_message_callback));
    std::vector<blink::WebMessagePort> message_ports;
    message_ports.push_back(std::move(page_port));
    cast_web_contents_->PostMessageToMainFrame(
        gurl.GetOrigin().spec(), kHelloMsg, std::move(message_ports));
    run_loop.Run();
  }
  // Test whether we could receive the right response from the page after we
  // send messages through |platform_port|.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auto received_message_callback = base::BindOnce(
        [](base::OnceClosure loop_quit_closure, std::string port_msg,
           base::Optional<blink::WebMessagePort> incoming_port) {
          EXPECT_EQ("ack ping", port_msg);
          std::move(loop_quit_closure).Run();
        },
        std::move(quit_closure));
    message_receiver.WaitForNextIncomingMessage(
        std::move(received_message_callback));
    platform_port.PostMessage(
        blink::WebMessagePort::Message(base::UTF8ToUTF16(kPingMsg)));
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

  EXPECT_CALL(mock_cast_wc_observer_,
              UpdateTitle(base::ASCIIToUTF16(kOriginalTitle)));
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
                            base::ThreadTaskRunnerHandle::Get());

  // Make sure we could post a MessagePort (ScopedMessagePipeHandle) to
  // the page.
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    auto received_message_callback = base::BindOnce(
        [](base::OnceClosure loop_quit_closure, std::string port_msg,
           base::Optional<blink::WebMessagePort> incoming_port) {
          EXPECT_EQ("got_port", port_msg);
          std::move(loop_quit_closure).Run();
        },
        std::move(quit_closure));
    message_receiver.WaitForNextIncomingMessage(
        std::move(received_message_callback));
    std::vector<blink::WebMessagePort> message_ports;
    message_ports.push_back(std::move(page_port));
    cast_web_contents_->PostMessageToMainFrame(
        gurl.GetOrigin().spec(), kHelloMsg, std::move(message_ports));
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
  auto run_loop = std::make_unique<base::RunLoop>();
  auto quit_closure = [&run_loop]() {
    if (run_loop->running()) {
      run_loop->QuitWhenIdle();
    }
  };

  // ===========================================================================
  // Test: Set a value using ExecuteJavaScript with empty callback, and then use
  // ExecuteJavaScript with callback to retrieve that value.
  // ===========================================================================
  constexpr char kSoyMilkJsonStringLiteral[] = "\"SoyMilk\"";

  // Load page with title "hello":
  GURL gurl{embedded_test_server()->GetURL("/title1.html")};
  {
    InSequence seq;
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(
            cast_web_contents_.get(), CastWebContents::PageState::LOADING)));
    EXPECT_CALL(
        mock_cast_wc_observer_,
        OnPageStateChanged(CheckPageState(cast_web_contents_.get(),
                                          CastWebContents::PageState::LOADED)))
        .WillOnce(InvokeWithoutArgs(quit_closure));
  }
  cast_web_contents_->LoadUrl(gurl);
  run_loop->Run();

  // Execute with empty callback.
  cast_web_contents_->ExecuteJavaScript(
      base::UTF8ToUTF16(
          base::StringPrintf("const the_var = %s;", kSoyMilkJsonStringLiteral)),
      base::DoNothing());

  // Execute a script snippet to return the variable's value.
  base::RunLoop run_loop2;
  cast_web_contents_->ExecuteJavaScript(
      base::UTF8ToUTF16("the_var;"),
      base::BindLambdaForTesting([&](base::Value result_value) {
        std::string result_json;
        ASSERT_TRUE(base::JSONWriter::Write(result_value, &result_json));
        EXPECT_EQ(result_json, kSoyMilkJsonStringLiteral);
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

// Helper for the test below. This exposes two interfaces, TestAdder and
// TestDoubler. TestAdder is exposed only through a binder (see MakeAdderBinder)
// which the test will register in the CastWebContents' binder_registry().
// TestDoubler is exposed only through an InterfaceProvider, registered with the
// CastWebContents using RegisterInterfaceProvider.
class TestInterfaceProvider : public service_manager::mojom::InterfaceProvider,
                              public mojom::TestAdder,
                              public mojom::TestDoubler {
 public:
  TestInterfaceProvider()
      : provider_(receiver_.BindNewPipeAndPassRemote(),
                  base::SequencedTaskRunnerHandle::Get()) {}
  ~TestInterfaceProvider() override = default;

  size_t num_adders() const { return adders_.size(); }
  size_t num_doublers() const { return doublers_.size(); }

  service_manager::InterfaceProvider* interface_provider() {
    return &provider_;
  }

  base::RepeatingCallback<void(mojo::PendingReceiver<mojom::TestAdder>)>
  MakeAdderBinder() {
    return base::BindLambdaForTesting(
        [this](mojo::PendingReceiver<mojom::TestAdder> receiver) {
          adders_.Add(this, std::move(receiver));
          OnRequestHandled();
        });
  }

  // Waits for some number of new interface binding requests to be dispatched
  // and then invokes `callback`.
  void WaitForRequests(size_t n, base::OnceClosure callback) {
    wait_callback_ = std::move(callback);
    num_requests_to_wait_for_ = n;
  }

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override {
    if (interface_name == mojom::TestDoubler::Name_) {
      doublers_.Add(this, mojo::PendingReceiver<mojom::TestDoubler>(
                              std::move(interface_pipe)));
      OnRequestHandled();
    }
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
    if (num_requests_to_wait_for_ == 0)
      return;
    DCHECK(wait_callback_);
    if (--num_requests_to_wait_for_ == 0)
      std::move(wait_callback_).Run();
  }

  mojo::Receiver<service_manager::mojom::InterfaceProvider> receiver_{this};
  service_manager::InterfaceProvider provider_;
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
  cast_web_contents_->binder_registry()->AddInterface(
      provider.MakeAdderBinder());
  cast_web_contents_->RegisterInterfaceProvider(
      CastWebContents::InterfaceSet{mojom::TestDoubler::Name_},
      provider.interface_provider());

  // First verify that both interfaces are reachable using the deprecated
  // WebContents path, which is triggered only by renderer-side use of
  // RenderFrame::GetRemoteInterfaces(). Since poking renderer state in browser
  // tests is challenging, we simply simulate the resulting WebContentsObbserver
  // calls here instead and verify end-to-end connection for each interface.
  content::RenderFrameHost* main_frame =
      cast_web_contents_->web_contents()->GetMainFrame();
  mojo::Remote<mojom::TestAdder> adder;
  mojo::ScopedMessagePipeHandle adder_receiver_pipe =
      adder.BindNewPipeAndPassReceiver().PassPipe();
  cast_web_contents_->OnInterfaceRequestFromFrame(
      main_frame, mojom::TestAdder::Name_, &adder_receiver_pipe);
  mojo::Remote<mojom::TestDoubler> doubler;
  mojo::ScopedMessagePipeHandle doubler_receiver_pipe =
      doubler.BindNewPipeAndPassReceiver().PassPipe();
  cast_web_contents_->OnInterfaceRequestFromFrame(
      main_frame, mojom::TestDoubler::Name_, &doubler_receiver_pipe);

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
