// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_service_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "components/cast_receiver/browser/public/content_window_controls.h"
#include "components/cast_receiver/browser/public/runtime_application.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

using ::testing::_;

class FakeCastContentWindow : public CastContentWindow {
 public:
  explicit FakeCastContentWindow(mojom::CastWebViewParamsPtr params)
      : CastContentWindow(std::move(params)) {}
  ~FakeCastContentWindow() override = default;

  void CreateWindow(mojom::ZOrder z_order,
                    VisibilityPriority visibility_priority) override {}
  void GrantScreenAccess() override {}
  void RevokeScreenAccess() override {}
  void RequestVisibility(VisibilityPriority visibility_priority) override {}
  void EnableTouchInput(bool enabled) override {}
};

class FakeCastWebView : public CastWebView {
 public:
  explicit FakeCastWebView(std::unique_ptr<CastContentWindow> window)
      : window_(std::move(window)) {}
  ~FakeCastWebView() override = default;

  CastContentWindow* window() const override { return window_.get(); }
  content::WebContents* web_contents() const override { return nullptr; }
  CastWebContents* cast_web_contents() override { return nullptr; }
  base::TimeDelta shutdown_delay() const override { return base::TimeDelta(); }
  void OwnerDestroyed() override {}

  void ResetWindow() { window_.reset(); }
  base::WeakPtr<FakeCastWebView> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<CastContentWindow> window_;
  base::WeakPtrFactory<FakeCastWebView> weak_factory_{this};
};

class FakeCastWebViewFactory : public CastWebViewFactory {
 public:
  FakeCastWebViewFactory()
      : CastWebViewFactory(reinterpret_cast<content::BrowserContext*>(this)) {}
  ~FakeCastWebViewFactory() override = default;

  std::unique_ptr<CastWebView> CreateWebView(
      mojom::CastWebViewParamsPtr params,
      CastWebService* web_service) override {
    auto window = std::make_unique<FakeCastContentWindow>(std::move(params));
    auto web_view = std::make_unique<FakeCastWebView>(std::move(window));
    last_web_view_ = web_view->GetWeakPtr();
    return web_view;
  }

  FakeCastWebView* last_web_view() const { return last_web_view_.get(); }

 private:
  base::WeakPtr<FakeCastWebView> last_web_view_;
};

class MockRuntimeApplication : public cast_receiver::RuntimeApplication {
 public:
  MockRuntimeApplication() = default;
  ~MockRuntimeApplication() override {
    if (on_destroy_) {
      std::move(on_destroy_).Run();
    }
  }

  void set_on_destroy(base::OnceClosure on_destroy) {
    on_destroy_ = std::move(on_destroy);
  }

  MOCK_METHOD(const std::string&, GetDisplayName, (), (const, override));
  MOCK_METHOD(const std::string&, GetAppId, (), (const, override));
  MOCK_METHOD(const std::string&, GetCastSessionId, (), (const, override));
  MOCK_METHOD(bool, IsStreamingApplication, (), (const, override));
  MOCK_METHOD(bool, IsApplicationRunning, (), (const, override));
  MOCK_METHOD(void, Load, (StatusCallback), (override));
  MOCK_METHOD(void, Launch, (StatusCallback), (override));
  MOCK_METHOD(void, Stop, (StatusCallback), (override));
  MOCK_METHOD(void,
              SetUrlRewriteRules,
              (url_rewrite::mojom::UrlRequestRewriteRulesPtr),
              (override));
  MOCK_METHOD(void, SetMediaBlocking, (bool, bool), (override));
  MOCK_METHOD(void, SetVisibility, (bool), (override));
  MOCK_METHOD(void, SetTouchInputEnabled, (bool), (override));

 private:
  base::OnceClosure on_destroy_;
};

class RuntimeApplicationServiceImplTest : public testing::Test {
 public:
  RuntimeApplicationServiceImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    web_service_ = std::make_unique<CastWebService>(
        reinterpret_cast<content::BrowserContext*>(this), nullptr);
    web_service_->OverrideWebViewFactory(&web_view_factory_);

    auto runtime_application =
        std::make_unique<testing::NiceMock<MockRuntimeApplication>>();
    mock_runtime_app_ = runtime_application.get();
    ON_CALL(*mock_runtime_app_, GetCastSessionId())
        .WillByDefault(testing::ReturnRef(session_id_));
    ON_CALL(*mock_runtime_app_, GetAppId())
        .WillByDefault(testing::ReturnRef(app_id_));
    ON_CALL(*mock_runtime_app_, GetDisplayName())
        .WillByDefault(testing::ReturnRef(app_id_));
    ON_CALL(*mock_runtime_app_, IsApplicationRunning())
        .WillByDefault(testing::ReturnPointee(&is_application_running_));
    ON_CALL(*mock_runtime_app_, Load(_))
        .WillByDefault(
            [this](cast_receiver::RuntimeApplication::StatusCallback callback) {
              is_application_running_ = true;
              std::move(callback).Run(cast_receiver::OkStatus());
            });
    ON_CALL(*mock_runtime_app_, Stop(_))
        .WillByDefault(
            [this](cast_receiver::RuntimeApplication::StatusCallback callback) {
              is_application_running_ = false;
              std::move(callback).Run(cast_receiver::OkStatus());
            });

    cast::common::ApplicationConfig config;
    config.set_app_id(app_id_);

    service_impl_ = std::make_unique<RuntimeApplicationServiceImpl>(
        std::move(runtime_application), std::move(config),
        base::SingleThreadTaskRunner::GetCurrentDefault(), *web_service_);
  }

  ~RuntimeApplicationServiceImplTest() override {
    mock_runtime_app_ = nullptr;
    service_impl_.reset();
    web_service_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  const std::string app_id_ = "test_app_id";
  const std::string session_id_ = "test_session_id";
  bool is_application_running_ = false;
  FakeCastWebViewFactory web_view_factory_;
  std::unique_ptr<CastWebService> web_service_;
  raw_ptr<MockRuntimeApplication> mock_runtime_app_ = nullptr;
  std::unique_ptr<RuntimeApplicationServiceImpl> service_impl_;
};

TEST_F(RuntimeApplicationServiceImplTest,
       WindowDestructionStopsRuntimeApplicationSafely) {
  cast::runtime::LoadApplicationRequest load_request;
  load_request.mutable_application_config()->set_app_id("test_app_id");
  load_request.set_cast_session_id("test_session_id");
  load_request.mutable_runtime_application_service_info()->set_grpc_endpoint(
      "localhost:0");

  EXPECT_CALL(*mock_runtime_app_, SetUrlRewriteRules(_)).Times(1);
  EXPECT_CALL(*mock_runtime_app_, Load(_))
      .WillOnce(
          [this](cast_receiver::RuntimeApplication::StatusCallback callback) {
            is_application_running_ = true;
            std::move(callback).Run(cast_receiver::OkStatus());
          });
  service_impl_->Load(load_request, base::DoNothing());
  ASSERT_NE(web_view_factory_.last_web_view(), nullptr);

  cast_receiver::ContentWindowControls* controls =
      service_impl_->GetContentWindowControls();
  ASSERT_NE(controls, nullptr);

  EXPECT_CALL(*mock_runtime_app_, Stop(_))
      .WillOnce([this](cast_receiver::RuntimeApplication::StatusCallback) {
        is_application_running_ = false;
        service_impl_->NotifyApplicationStopped(
            cast_receiver::EmbedderApplication::ApplicationStopReason::
                kUserRequest,
            0);
      });
  web_view_factory_.last_web_view()->ResetWindow();

  // Even after the underlying CastContentWindow is destroyed,
  // GetContentWindowControls() must continue returning the existing controls
  // instance so RuntimeApplicationBase::StopApplication() can unregister its
  // VisibilityChangeObserver without crashing.
  EXPECT_EQ(service_impl_->GetContentWindowControls(), controls);
  controls->ShowWindow();
  controls->HideWindow();

  // Once the current call stack unwinds, the posted cleanup task from
  // NotifyApplicationStopped() must tear down the CastWebView and its
  // ContentWindowControls, and message port service must remain reset.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(web_view_factory_.last_web_view(), nullptr);
  EXPECT_EQ(service_impl_->GetContentWindowControls(), nullptr);
  EXPECT_EQ(service_impl_->GetMessagePortService(), nullptr);
}

TEST_F(RuntimeApplicationServiceImplTest, NotifyWindowDestroyedIsIdempotent) {
  class CountingWindowObserver : public CastContentWindow::Observer {
   public:
    explicit CountingWindowObserver(CastContentWindow* window)
        : window_(window) {
      window_->AddObserver(this);
    }
    ~CountingWindowObserver() override {
      if (window_) {
        window_->RemoveObserver(this);
      }
    }
    void OnVisibilityChange(VisibilityType visibility_type) override {}
    void OnWindowDestroyed() override {
      destroyed_count_++;
      if (window_) {
        window_->RemoveObserver(this);
        window_ = nullptr;
      }
    }
    int destroyed_count() const { return destroyed_count_; }

   private:
    raw_ptr<CastContentWindow> window_;
    int destroyed_count_ = 0;
  };

  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  auto window = std::make_unique<FakeCastContentWindow>(std::move(params));
  CountingWindowObserver observer(window.get());

  window->NotifyWindowDestroyed();
  window->NotifyWindowDestroyed();
  EXPECT_EQ(observer.destroyed_count(), 1);

  window.reset();
  EXPECT_EQ(observer.destroyed_count(), 1);
}

TEST_F(RuntimeApplicationServiceImplTest,
       DestructionWhileRunningStopsApplicationSafely) {
  cast::runtime::LoadApplicationRequest load_request;
  load_request.mutable_application_config()->set_app_id("test_app_id");
  load_request.set_cast_session_id("test_session_id");
  load_request.mutable_runtime_application_service_info()->set_grpc_endpoint(
      "localhost:0");

  EXPECT_CALL(*mock_runtime_app_, SetUrlRewriteRules(_)).Times(1);
  EXPECT_CALL(*mock_runtime_app_, Load(_))
      .WillOnce(
          [this](cast_receiver::RuntimeApplication::StatusCallback callback) {
            is_application_running_ = true;
            std::move(callback).Run(cast_receiver::OkStatus());
          });
  service_impl_->Load(load_request, base::DoNothing());
  EXPECT_TRUE(is_application_running_);

  cast_receiver::EmbedderApplication* embedder = service_impl_.get();
  cast_receiver::ContentWindowControls* controls =
      embedder->GetContentWindowControls();
  ASSERT_NE(controls, nullptr);

  auto stop_app = [this, embedder, controls]() {
    if (!is_application_running_) {
      return;
    }
    is_application_running_ = false;
    // Mirror RuntimeApplicationBase::StopApplication(), which accesses
    // GetContentWindowControls() and calls NotifyApplicationStopped() on the
    // embedder.
    EXPECT_EQ(embedder->GetContentWindowControls(), controls);
    embedder->NotifyApplicationStopped(
        cast_receiver::EmbedderApplication::ApplicationStopReason::kUserRequest,
        0);
  };

  EXPECT_CALL(*mock_runtime_app_, Stop(_))
      .WillOnce(
          [&stop_app](cast_receiver::RuntimeApplication::StatusCallback cb) {
            stop_app();
            std::move(cb).Run(cast_receiver::OkStatus());
          });

  // Mirror WebRuntimeApplication::~WebRuntimeApplication() and
  // StreamingRuntimeApplication::~StreamingRuntimeApplication(), which call
  // StopApplication() from their destructors if still running.
  mock_runtime_app_->set_on_destroy(base::BindLambdaForTesting(stop_app));

  // Destroying RuntimeApplicationServiceImpl while the application is still
  // running (e.g. during SIGTERM shutdown) must stop the application before
  // member destruction sets runtime_application_ to nullptr or destroys
  // content_window_controls_.
  mock_runtime_app_ = nullptr;
  service_impl_.reset();
  EXPECT_FALSE(is_application_running_);
}

TEST_F(RuntimeApplicationServiceImplTest,
       CallbacksDuringMemberDestructionDoNotCrash) {
  cast::runtime::LoadApplicationRequest load_request;
  load_request.mutable_application_config()->set_app_id("test_app_id");
  load_request.set_cast_session_id("test_session_id");
  load_request.mutable_runtime_application_service_info()->set_grpc_endpoint(
      "localhost:0");

  EXPECT_CALL(*mock_runtime_app_, SetUrlRewriteRules(_)).Times(1);
  EXPECT_CALL(*mock_runtime_app_, Load(_))
      .WillOnce(
          [this](cast_receiver::RuntimeApplication::StatusCallback callback) {
            is_application_running_ = true;
            std::move(callback).Run(cast_receiver::OkStatus());
          });
  service_impl_->Load(load_request, base::DoNothing());
  ASSERT_NE(service_impl_->GetContentWindowControls(), nullptr);

  // Simulate an application that remains non-running when
  // ~RuntimeApplicationServiceImpl begins, or triggers callbacks during member
  // destruction after runtime_application_ has been set to nullptr by
  // std::unique_ptr::reset().
  is_application_running_ = false;
  RuntimeApplicationServiceImpl* service = service_impl_.get();
  mock_runtime_app_->set_on_destroy(base::BindLambdaForTesting([service]() {
    EXPECT_EQ(service->app_id(), "test_app_id");
    EXPECT_EQ(service->GetContentWindowControls(), nullptr);
    service->NotifyApplicationStopped(
        cast_receiver::EmbedderApplication::ApplicationStopReason::kUserRequest,
        0);
  }));

  mock_runtime_app_ = nullptr;
  service_impl_.reset();
}

TEST_F(RuntimeApplicationServiceImplTest,
       LaunchFailsPreconditionWhenWebViewClosed) {
  cast::runtime::LaunchApplicationRequest launch_request;
  launch_request.mutable_core_application_service_info()->set_grpc_endpoint(
      "localhost:0");

  cast_receiver::Status received_status = cast_receiver::OkStatus();
  service_impl_->Launch(launch_request,
                        base::BindLambdaForTesting(
                            [&received_status](cast_receiver::Status status) {
                              received_status = std::move(status);
                            }));
  EXPECT_EQ(received_status.code(),
            cast_receiver::StatusCode::kFailedPrecondition);
}

TEST_F(RuntimeApplicationServiceImplTest,
       VisibilityChangedDoesNotCrashWithoutCoreAppStub) {
  // If core_app_stub_ is not initialized (because Launch wasn't called),
  // this should safely return early.
  service_impl_->NotifyApplicationVisibilityChanged(
      cast::common::Visibility::HIDDEN);
}

}  // namespace
}  // namespace chromecast
