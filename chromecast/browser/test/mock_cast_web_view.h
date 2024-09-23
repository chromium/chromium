// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMECAST_BROWSER_TEST_MOCK_CAST_WEB_VIEW_H_
#define CHROMECAST_BROWSER_TEST_MOCK_CAST_WEB_VIEW_H_

#include <string_view>

#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/cast_web_view.h"
#include "components/url_rewrite/mojom/url_request_rewrite.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {

class MockCastWebContents : public CastWebContents {
 public:
  MockCastWebContents();
  ~MockCastWebContents() override;

  // CastWebContents implementation
  MOCK_METHOD(int, tab_id, (), (const, override));
  MOCK_METHOD(int, id, (), (const, override));
  MOCK_METHOD(content::WebContents*, web_contents, (), (const, override));
  MOCK_METHOD(PageState, page_state, (), (const, override));
  MOCK_METHOD(const media_control::MediaBlocker*,
              media_blocker,
              (),
              (const override));
  MOCK_METHOD(void, AddRendererFeatures, (base::Value::Dict), (override));
  MOCK_METHOD(void,
              SetInterfacesForRenderer,
              (mojo::PendingRemote<mojom::RemoteInterfaces>),
              (override));
  MOCK_METHOD(void,
              SetAppProperties,
              (const std::string&,
               const std::string&,
               bool,
               const GURL&,
               bool,
               const std::vector<int32_t>&,
               const std::vector<std::string>&),
              (override));
  MOCK_METHOD(void, SetGroupInfo, (const std::string&, bool), (override));
  MOCK_METHOD(void, LoadUrl, (const GURL&), (override));
  MOCK_METHOD(void, ClosePage, (), (override));
  MOCK_METHOD(void, Stop, (int), (override));
  MOCK_METHOD(void, SetWebVisibilityAndPaint, (bool), (override));
  MOCK_METHOD(void, BlockMediaLoading, (bool), (override));
  MOCK_METHOD(void, BlockMediaStarting, (bool), (override));
  MOCK_METHOD(void, EnableBackgroundVideoPlayback, (bool), (override));
  MOCK_METHOD(void,
              AddBeforeLoadJavaScript,
              (uint64_t, std::string_view),
              (override));
  MOCK_METHOD(void,
              PostMessageToMainFrame,
              (const std::string&,
               const std::string&,
               std::vector<blink::WebMessagePort>),
              (override));
  MOCK_METHOD(void,
              ExecuteJavaScript,
              (const std::u16string&, base::OnceCallback<void(base::Value)>),
              (override));
  MOCK_METHOD(void,
              ConnectToBindingsService,
              (mojo::PendingRemote<mojom::ApiBindings> api_bindings_remote),
              (override));
  MOCK_METHOD(void, SetEnabledForRemoteDebugging, (bool), (override));
  MOCK_METHOD(void, GetMainFramePid, (GetMainFramePidCallback), (override));
  MOCK_METHOD(InterfaceBundle*, local_interfaces, (), (override));
  MOCK_METHOD(bool, is_websql_enabled, (), (override));
  MOCK_METHOD(bool, is_mixer_audio_enabled, (), (override));

  bool TryBindReceiver(mojo::GenericPendingReceiver&) override;

 private:
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
  void OwnerDestroyed() override;

  MockCastWebContents* mock_cast_web_contents() {
    return mock_cast_web_contents_.get();
  }

  void Bind(mojo::PendingReceiver<mojom::CastWebContents> web_contents);

 private:
  std::unique_ptr<MockCastWebContents> mock_cast_web_contents_;
  mojo::Receiver<mojom::CastWebContents> cast_web_contents_receiver_;
};
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_TEST_MOCK_CAST_WEB_VIEW_H_
