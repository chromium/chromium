// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_TEST_MOCK_MEDIA_ROUTER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_TEST_MOCK_MEDIA_ROUTER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/browser/media_router_base.h"
#include "components/media_router/browser/media_router_debugger.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/media_router/browser/issue_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {
class BrowserContext;
}

namespace media_router {

// Media Router mock class. Used for testing purposes.
class MockMediaRouter : public MediaRouterBase {
 public:
  // This method can be passed into MediaRouterFactory::SetTestingFactory() to
  // make the factory return a MockMediaRouter.
  static std::unique_ptr<KeyedService> Create(content::BrowserContext* context);

  MockMediaRouter();
  ~MockMediaRouter() override;

  void Initialize() override {}

  // TODO(crbug.com/40524294): Use MOCK_METHOD directly once GMock gets the
  // move-only type support.
  void CreateRoute(const MediaSource::Id& source,
                   const MediaSink::Id& sink_id,
                   const url::Origin& origin,
                   content::WebContents* web_contents,
                   MediaRouteResponseCallback callback,
                   base::TimeDelta timeout) override {
    CreateRouteInternal(source, sink_id, origin, web_contents, callback,
                        timeout);
  }
  MOCK_METHOD6(CreateRouteInternal,
               void(const MediaSource::Id& source,
                    const MediaSink::Id& sink_id,
                    const url::Origin& origin,
                    content::WebContents* web_contents,
                    MediaRouteResponseCallback& callback,
                    base::TimeDelta timeout));

  void JoinRoute(const MediaSource::Id& source,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 content::WebContents* web_contents,
                 MediaRouteResponseCallback callback,
                 base::TimeDelta timeout) override {
    JoinRouteInternal(source, presentation_id, origin, web_contents, callback,
                      timeout);
  }
  MOCK_METHOD6(JoinRouteInternal,
               void(const MediaSource::Id& source,
                    const std::string& presentation_id,
                    const url::Origin& origin,
                    content::WebContents* web_contents,
                    MediaRouteResponseCallback& callback,
                    base::TimeDelta timeout));

  MOCK_METHOD1(DetachRoute, void(MediaRoute::Id route_id));
  MOCK_METHOD1(TerminateRoute, void(const MediaRoute::Id& route_id));
  MOCK_METHOD2(SendRouteMessage,
               void(const MediaRoute::Id& route_id,
                    const std::string& message));
  MOCK_METHOD2(SendRouteBinaryMessage,
               void(const MediaRoute::Id& route_id,
                    std::unique_ptr<std::vector<uint8_t>> data));
  MOCK_METHOD0(OnUserGesture, void());
  MOCK_METHOD1(OnPresentationSessionDetached,
               void(const MediaRoute::Id& route_id));
  base::CallbackListSubscription AddPresentationConnectionStateChangedCallback(
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionStateChangedCallback& callback)
      override {
    OnAddPresentationConnectionStateChangedCallbackInvoked(callback);
    return MediaRouterBase::AddPresentationConnectionStateChangedCallback(
        route_id, callback);
  }
  MOCK_CONST_METHOD0(GetCurrentRoutes, std::vector<MediaRoute>());
  MOCK_METHOD1(GetFlingingController,
               std::unique_ptr<media::FlingingController>(
                   const MediaRoute::Id& route_id));

#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD1(GetMirroringMediaControllerHost,
               MirroringMediaControllerHost*(const MediaRoute::Id& route_id));
  IssueManager* GetIssueManager() override { return &issue_manager_; }
  MOCK_METHOD3(GetMediaController,
               void(const MediaRoute::Id& route_id,
                    mojo::PendingReceiver<mojom::MediaController> controller,
                    mojo::PendingRemote<mojom::MediaStatusObserver> observer));
  MOCK_CONST_METHOD2(
      GetProviderState,
      void(mojom::MediaRouteProviderId provider_id,
           mojom::MediaRouteProvider::GetStateCallback callback));
  MOCK_CONST_METHOD0(GetLogs, base::Value());
  MOCK_CONST_METHOD0(GetState, base::Value::Dict());
  MOCK_METHOD0(GetLogger, LoggerImpl*());
  MOCK_METHOD(MediaRouterDebugger&, GetDebugger, (), (override));
#endif  // !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD1(OnAddPresentationConnectionStateChangedCallbackInvoked,
               void(const content::PresentationConnectionStateChangedCallback&
                        callback));
  MOCK_METHOD1(RegisterMediaSinksObserver, bool(MediaSinksObserver* observer));
  MOCK_METHOD1(UnregisterMediaSinksObserver,
               void(MediaSinksObserver* observer));
  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) override {
    routes_observers_.AddObserver(observer);
  }
  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) override {
    routes_observers_.RemoveObserver(observer);
  }
  MOCK_METHOD1(RegisterPresentationConnectionMessageObserver,
               void(PresentationConnectionMessageObserver* observer));
  MOCK_METHOD1(UnregisterPresentationConnectionMessageObserver,
               void(PresentationConnectionMessageObserver* observer));
  MOCK_METHOD0(GetMediaSinkServiceStatus, std::string());

  base::ObserverList<MediaRoutesObserver>& routes_observers() {
    return routes_observers_;
  }

 protected:
  base::ObserverList<MediaRoutesObserver> routes_observers_;

 private:
#if !BUILDFLAG(IS_ANDROID)
  IssueManager issue_manager_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_TEST_MOCK_MEDIA_ROUTER_H_
