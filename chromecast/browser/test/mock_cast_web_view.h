// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMECAST_BROWSER_TEST_MOCK_CAST_WEB_VIEW_H_
#define CHROMECAST_BROWSER_TEST_MOCK_CAST_WEB_VIEW_H_

#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/cast_web_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {

class MockCastWebContents : public CastWebContents {
 public:
  MockCastWebContents();
  ~MockCastWebContents();

  // CastWebContents implementation
  MOCK_METHOD(int, tab_id, (), (const, override));
  MOCK_METHOD(int, id, (), (const, override));
  MOCK_METHOD(content::WebContents*, web_contents, (), (const, override));
  MOCK_METHOD(PageState, page_state, (), (const, override));
  MOCK_METHOD(base::Optional<pid_t>,
              GetMainFrameRenderProcessPid,
              (),
              (const, override));
  MOCK_METHOD(void,
              AddRendererFeatures,
              (std::vector<RendererFeature>),
              (override));
  MOCK_METHOD(void, AllowWebAndMojoWebUiBindings, (), (override));
  MOCK_METHOD(void, ClearRenderWidgetHostView, (), (override));
  MOCK_METHOD(void, LoadUrl, (const GURL&), (override));
  MOCK_METHOD(void, ClosePage, (), (override));
  MOCK_METHOD(void, Stop, (int), (override));
  MOCK_METHOD(void, SetWebVisibilityAndPaint, (bool), (override));
  MOCK_METHOD(void, BlockMediaLoading, (bool), (override));
  MOCK_METHOD(void, BlockMediaStarting, (bool), (override));
  MOCK_METHOD(void, EnableBackgroundVideoPlayback, (bool), (override));
  MOCK_METHOD(on_load_script_injector::OnLoadScriptInjectorHost<std::string>*,
              script_injector,
              (),
              (override));
  MOCK_METHOD(void, InjectScriptsIntoMainFrame, (), (override));
  MOCK_METHOD(void,
              PostMessageToMainFrame,
              (const std::string&,
               const std::string&,
               std::vector<blink::WebMessagePort>),
              (override));
  MOCK_METHOD(void,
              ExecuteJavaScript,
              (const base::string16&, base::OnceCallback<void(base::Value)>),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(void, SetEnabledForRemoteDebugging, (bool), (override));
  MOCK_METHOD(void,
              RegisterInterfaceProvider,
              (const InterfaceSet&, service_manager::InterfaceProvider*),
              (override));
  MOCK_METHOD(bool, is_websql_enabled, (), (override));
  MOCK_METHOD(bool, is_mixer_audio_enabled, (), (override));
  MOCK_METHOD(bool, can_bind_interfaces, (), (override));

  service_manager::BinderRegistry* binder_registry() override;
  bool TryBindReceiver(mojo::GenericPendingReceiver&) override;

 private:
  service_manager::BinderRegistry registry_;
};

class MockCastWebView : public CastWebView {
 public:
  MockCastWebView();
  ~MockCastWebView() override;

  // CastWebView implementation
  CastContentWindow* window() const override;
  content::WebContents* web_contents() const override;
  CastWebContents* cast_web_contents() override;
  base::TimeDelta shutdown_delay() const override;
  void ForceClose() override;
  void InitializeWindow(mojom::ZOrder z_order,
                        VisibilityPriority initial_priority) override;
  void GrantScreenAccess() override;
  void RevokeScreenAccess() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  MockCastWebContents* mock_cast_web_contents() {
    return mock_cast_web_contents_.get();
  }

 private:
  std::unique_ptr<MockCastWebContents> mock_cast_web_contents_;
};
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_TEST_MOCK_CAST_WEB_VIEW_H_
