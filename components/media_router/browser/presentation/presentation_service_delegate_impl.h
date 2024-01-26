// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_PRESENTATION_SERVICE_DELEGATE_IMPL_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_PRESENTATION_SERVICE_DELEGATE_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_observers.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class PresentationScreenAvailabilityListener;
class WebContents;
}  // namespace content

namespace media_router {

class MediaRoute;
class PresentationFrame;
class RouteRequestResult;

// Implementation of PresentationServiceDelegate that interfaces an instance of
// WebContents with the Chrome Media Router. It uses the Media Router to handle
// presentation API calls forwarded from PresentationServiceImpl. In addition,
// it also provides default presentation URL that is required for creating
// browser-initiated presentations.  It is scoped to the lifetime of a
// WebContents, and is managed by the associated WebContents.
// It is accessed through the WebContentsPresentationManager interface by
// clients (e.g. the UI code) that is interested in the presentation status of
// the WebContents, but not in other aspects such as the render frame.
class PresentationServiceDelegateImpl
    : public content::WebContentsUserData<PresentationServiceDelegateImpl>,
      public content::ControllerPresentationServiceDelegate,
      public WebContentsPresentationManager {
 public:
  // Retrieves the instance of PresentationServiceDelegateImpl that was attached
  // to the specified WebContents.  If no instance was attached, creates one,
  // and attaches it to the specified WebContents.
  static PresentationServiceDelegateImpl* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  PresentationServiceDelegateImpl(const PresentationServiceDelegateImpl&) =
      delete;
  PresentationServiceDelegateImpl& operator=(
      const PresentationServiceDelegateImpl&) = delete;

  ~PresentationServiceDelegateImpl() override;

  // content::PresentationServiceDelegate implementation.
  void AddObserver(
      int render_process_id,
      int render_frame_id,
      content::PresentationServiceDelegate::Observer* observer) override;
  void RemoveObserver(int render_process_id, int render_frame_id) override;
  bool AddScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      content::PresentationScreenAvailabilityListener* listener) override;
  void RemoveScreenAvailabilityListener(
      int render_process_id,
      int render_frame_id,
      content::PresentationScreenAvailabilityListener* listener) override;
  void Reset(int render_process_id, int render_frame_id) override;
  void SetDefaultPresentationUrls(
      const content::PresentationRequest& request,
      content::DefaultPresentationConnectionCallback callback) override;
  void StartPresentation(
      const content::PresentationRequest& request,
      content::PresentationConnectionCallback success_cb,
      content::PresentationConnectionErrorCallback error_cb) override;
  void ReconnectPresentation(
      const content::PresentationRequest& request,
      const std::string& presentation_id,
      content::PresentationConnectionCallback success_cb,
      content::PresentationConnectionErrorCallback error_cb) override;
  void CloseConnection(int render_process_id,
                       int render_frame_id,
                       const std::string& presentation_id) override;
  void Terminate(int render_process_id,
                 int render_frame_id,
                 const std::string& presentation_id) override;
  std::unique_ptr<media::FlingingController> GetFlingingController(
      int render_process_id,
      int render_frame_id,
      const std::string& presentation_id) override;
  void ListenForConnectionStateChange(
      int render_process_id,
      int render_frame_id,
      const blink::mojom::PresentationInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb) override;

  // WebContentsPresentationManager implementation.
  void AddObserver(content::PresentationObserver* observer) override;
  void RemoveObserver(content::PresentationObserver* observer) override;
  bool HasDefaultPresentationRequest() const override;
  const content::PresentationRequest& GetDefaultPresentationRequest()
      const override;
  void OnPresentationResponse(const content::PresentationRequest& request,
                              mojom::RoutePresentationConnectionPtr connection,
                              const RouteRequestResult& result) override;
  std::vector<MediaRoute> GetMediaRoutes() override;
  base::WeakPtr<WebContentsPresentationManager> GetWeakPtr() override;

  // Returns the WebContents that owns this instance.
  content::WebContents* web_contents() { return &GetWebContents(); }

  bool HasScreenAvailabilityListenerForTest(
      int render_process_id,
      int render_frame_id,
      const MediaSource::Id& source_id) const;

  void set_start_presentation_cb(
      base::RepeatingCallback<void(std::unique_ptr<StartPresentationContext>)>
          callback) {
    start_presentation_cb_ = std::move(callback);
  }

 private:
  friend class content::WebContentsUserData<PresentationServiceDelegateImpl>;
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           DelegateObservers);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           SetDefaultPresentationUrl);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           DefaultPresentationRequestObserver);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           DefaultPresentationUrlCallback);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           TestCloseConnectionForLocalPresentation);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           ConnectToLocalPresentation);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           ConnectToPresentation);

  explicit PresentationServiceDelegateImpl(content::WebContents* web_contents);

  PresentationFrame* GetOrAddPresentationFrame(
      const content::GlobalRenderFrameHostId& render_frame_host_id);

  void OnJoinRouteResponse(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      const GURL& presentation_url,
      const std::string& presentation_id,
      content::PresentationConnectionCallback success_cb,
      content::PresentationConnectionErrorCallback error_cb,
      mojom::RoutePresentationConnectionPtr connection,
      const RouteRequestResult& result);

  void OnStartPresentationSucceeded(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      content::PresentationConnectionCallback success_cb,
      const blink::mojom::PresentationInfo& new_presentation_info,
      mojom::RoutePresentationConnectionPtr connection,
      const MediaRoute& route);

  // Notifies the PresentationFrame of |render_frame_host_id| that a
  // presentation and its corresponding MediaRoute has been created.
  // The PresentationFrame will be created if it does not already exist.
  void AddPresentation(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      const blink::mojom::PresentationInfo& presentation_info,
      const MediaRoute& route);

  // Notifies the PresentationFrame of |render_frame_host_id| that a
  // presentation and its corresponding MediaRoute has been removed.
  void RemovePresentation(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      const std::string& presentation_id);

  // Clears the default presentation request for the owning WebContents and
  // notifies observers of changes. Also resets
  // |default_presentation_started_callback_|.
  void ClearDefaultPresentationRequest();

  // Returns the ID of the route corresponding to |presentation_id| in the given
  // frame, or empty if no such route exist.
  MediaRoute::Id GetRouteId(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      const std::string& presentation_id) const;

  // Ensures that |connection| contains a valid pair of
  // blink::mojom::PresentationConnection{PtrInfo,Request} objects which will be
  // used for all Presentation API communication in a newly-connected
  // presentation.
  void EnsurePresentationConnection(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      const blink::mojom::PresentationInfo& presentation_info,
      mojom::RoutePresentationConnectionPtr* connection);

  void NotifyDefaultPresentationChanged(
      const content::PresentationRequest* request);
  void NotifyMediaRoutesChanged();

  // Invoked by the MR when a Presentation Connection state changes in a frame.
  // It calls |RemovePresentation()| when the connection is closed/terminated.
  void OnConnectionStateChanged(
      const content::GlobalRenderFrameHostId& render_frame_host_id,
      const blink::mojom::PresentationInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb,
      const content::PresentationConnectionStateChangeInfo& info);

  // Reference to the associated browser profile's MediaRouter instance.
  raw_ptr<MediaRouter> router_;

  // References to the observers listening for changes to the default
  // presentation and presentation MediaRoutes associated with the
  // WebContents.
  base::ObserverList<content::PresentationObserver> presentation_observers_;

  // Default presentation request for the owning WebContents.
  std::optional<content::PresentationRequest> default_presentation_request_;

  // Callback to invoke when the default presentation has started.
  content::DefaultPresentationConnectionCallback
      default_presentation_started_callback_;

  // If this callback is set when a request to start a presentation is made,
  // it is called instead of showing the Media Router dialog.
  base::RepeatingCallback<void(std::unique_ptr<StartPresentationContext>)>
      start_presentation_cb_;

  // Maps a frame identifier to a PresentationFrame object for frames
  // that are using Presentation API.
  std::unordered_map<content::GlobalRenderFrameHostId,
                     std::unique_ptr<PresentationFrame>,
                     content::GlobalRenderFrameHostIdHasher>
      presentation_frames_;

  PresentationServiceDelegateObservers observers_;

  base::WeakPtrFactory<PresentationServiceDelegateImpl> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_PRESENTATION_SERVICE_DELEGATE_IMPL_H_
