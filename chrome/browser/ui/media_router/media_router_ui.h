// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_

#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes_observer.h"
#include "chrome/browser/ui/media_router/presentation_request_source_observer.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/mirroring_media_controller_host.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/media_source.h"

namespace content {
struct PresentationRequest;
class WebContents;
}  // namespace content

namespace U_ICU_NAMESPACE {
class Collator;
}

namespace media_router {

class MediaRoute;
class MediaRouter;
class MediaRoutesObserver;
class MediaSink;
class RouteRequestResult;
class WebContentsDisplayObserver;

// Functions as an intermediary between MediaRouter and Views Cast dialog.
class MediaRouterUI : public CastDialogController,
                      public MediaSinkWithCastModesObserver,
                      public PresentationRequestSourceObserver,
                      public MirroringMediaControllerHost::Observer {
 public:
  // MediaRouterUI's are typically created with one of the CreateWith* methods
  // below.
  explicit MediaRouterUI(
      std::unique_ptr<MediaRouteStarter> media_route_starter);

  MediaRouterUI(const MediaRouterUI&) = delete;
  MediaRouterUI& operator=(const MediaRouterUI&) = delete;

  ~MediaRouterUI() override;

  // Creates a |MediaRouterUI| (e.g. starts listening for MediaSinks) for
  // targeting the default MediaSource (if any) of |initiator|. The contents
  // of the UI will change as the default MediaSource changes. If there is a
  // default MediaSource, then PRESENTATION MediaCastMode will be added to
  // |cast_modes_|.
  static std::unique_ptr<MediaRouterUI> CreateWithDefaultMediaSource(
      content::WebContents* initiator);
  // Initializes mirroring sources of the tab in addition to what is done by
  // |CreateWithDefaultMediaSource()|.
  static std::unique_ptr<MediaRouterUI>
  CreateWithDefaultMediaSourceAndMirroring(content::WebContents* initiator);

  // Initializes internal state targeting the presentation specified in
  // |context|. This is different from CreateWithDefaultMediaSource*() in that
  // it does not listen for default media source changes, as the UI is fixed to
  // the source in |context|. Init* methods can only be called once.
  // |context|: Context object for the PresentationRequest. This instance will
  // take ownership of it. Must not be null.
  static std::unique_ptr<MediaRouterUI> CreateWithStartPresentationContext(
      content::WebContents* initiator,
      std::unique_ptr<StartPresentationContext> context);
  // Initializes mirroring sources of the tab in addition to what is done by
  // |CreateWithStartPresentationContext()|.
  static std::unique_ptr<MediaRouterUI>
  CreateWithStartPresentationContextAndMirroring(
      content::WebContents* initiator,
      std::unique_ptr<StartPresentationContext> context);
  // Initializes RemotePlayback source only.
  static std::unique_ptr<MediaRouterUI> CreateWithMediaSessionRemotePlayback(
      content::WebContents* initiator,
      media::VideoCodec video_codec,
      media::AudioCodec audio_codec);

  // CastDialogController:
  void AddObserver(CastDialogController::Observer* observer) override;
  void RemoveObserver(CastDialogController::Observer* observer) override;
  void StartCasting(const std::string& sink_id,
                    MediaCastMode cast_mode) override;
  void StopCasting(const std::string& route_id) override;
  void ClearIssue(const Issue::Id& issue_id) override;
  void FreezeRoute(const std::string& route_id) override;
  void UnfreezeRoute(const std::string& route_id) override;
  // Note that |MediaRouterUI| should not be used after |TakeMediaRouteStarter|
  // is called. To enforce that, |TakeMediaRouteStarter| calls the destructor
  // callback given to |RegisterDestructor| to destroy itself.
  std::unique_ptr<MediaRouteStarter> TakeMediaRouteStarter() override;
  void RegisterDestructor(base::OnceClosure destructor) override;

  // Requests a route be created from the source mapped to
  // |cast_mode|, to the sink given by |sink_id|.
  // Returns true if a route request is successfully submitted.
  // |OnRouteResponseReceived()| will be invoked when the route request
  // completes.
  virtual bool CreateRoute(const MediaSink::Id& sink_id,
                           MediaCastMode cast_mode);

  // Calls MediaRouter to terminate the given route.
  void TerminateRoute(const MediaRoute::Id& route_id);

  // Returns a subset of |sinks_| that should be listed in the dialog. This
  // excludes the wired display that the initiator WebContents is on.
  // Also filters cloud sinks in incognito windows.
  std::vector<MediaSinkWithCastModes> GetEnabledSinks() const;

  // Calls MediaRouter to add the given issue.
  void AddIssue(const IssueInfo& issue);

  // Calls MediaRouter to remove the given issue.
  void RemoveIssue(const Issue::Id& issue_id);

  // Uses LoggerImpl to log current available sinks.
  void LogMediaSinkStatus();

  const std::vector<MediaRoute>& routes() const { return routes_; }
  content::WebContents* initiator() const {
    return media_route_starter()->GetWebContents();
  }

 private:
  friend class MediaRouterViewsUITest;
  friend class MediaRouterCastUiForTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, SetDialogHeader);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest,
                           UpdateSinksWhenDialogMovesToAnotherDisplay);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, NotifyObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, SinkFriendlyName);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, ConnectingState);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, DisconnectingState);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, AddAndRemoveIssue);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, ShowDomainForHangouts);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUIIncognitoTest,
                           HidesCloudSinksForIncognito);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, RouteCreationTimeout);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest,
                           RouteCreationTimeoutIssueTitle);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest,
                           DesktopMirroringFailsWhenDisallowedOnMac);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest,
                           RouteCreationLocalFileModeInTab);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           UIMediaRoutesObserverAssignsCurrentCastModes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           UIMediaRoutesObserverSkipsUnavailableCastModes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           UpdateSinksWhenDialogMovesToAnotherDisplay);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, OnFreezeInfoChanged);

  class WebContentsFullscreenOnLoadedObserver;

  // This class calls to refresh the UI when the highest priority issue is
  // updated.
  class UiIssuesObserver : public IssuesObserver {
   public:
    UiIssuesObserver(IssueManager* issue_manager, MediaRouterUI* ui);

    UiIssuesObserver(const UiIssuesObserver&) = delete;
    UiIssuesObserver& operator=(const UiIssuesObserver&) = delete;

    ~UiIssuesObserver() override;

    // IssuesObserver:
    void OnIssue(const Issue& issue) override;
    void OnIssuesCleared() override;

   private:
    // Reference back to the owning MediaRouterUI instance.
    const raw_ptr<MediaRouterUI> ui_;
  };

  class UIMediaRoutesObserver : public MediaRoutesObserver {
   public:
    using RoutesUpdatedCallback =
        base::RepeatingCallback<void(const std::vector<MediaRoute>&)>;
    UIMediaRoutesObserver(MediaRouter* router,
                          const RoutesUpdatedCallback& callback);

    UIMediaRoutesObserver(const UIMediaRoutesObserver&) = delete;
    UIMediaRoutesObserver& operator=(const UIMediaRoutesObserver&) = delete;

    ~UIMediaRoutesObserver() override;

    // MediaRoutesObserver:
    void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override;

   private:
    // Callback to the owning MediaRouterUI instance.
    RoutesUpdatedCallback callback_;
  };

  virtual void Init();

  // Removes |MediaRouteStarter| listeners and alerts observers that this
  // controller is now invalid.
  void DetachFromMediaRouteStarter();

  // PresentationRequestSourceObserver
  void OnSourceUpdated(std::u16string& source_name) override;

  // MirroringObserver
  void OnFreezeInfoChanged() override;

  // Called to update the dialog with the current list of of enabled sinks.
  void UpdateSinks();

  std::u16string GetSinkFriendlyNameFromId(const MediaSink::Id& sink_id);

  // Creates and sends an issue if route creation timed out.
  void SendIssueForRouteTimeout(
      MediaCastMode cast_mode,
      const MediaSink::Id& sink_id,
      const std::u16string& presentation_request_source_name);

  // Creates and sends an issue if casting fails due to lack of screen
  // permissions.
  void SendIssueForScreenPermission(const MediaSink::Id& sink_id);

  // Creates and sends an issue if casting fails for any reason other than
  // those above.
  void SendIssueForUnableToCast(MediaCastMode cast_mode,
                                const MediaSink::Id& sink_id);

  // Creates and sends an issue for notifying the user that the tab audio cannot
  // be mirrored from their device.
  void SendIssueForTabAudioNotSupported(const MediaSink::Id& sink_id);

  // Creates and sends an issue when the user rejects the Cast request on the
  // receiver device.
  void SendIssueForUserNotAllowed(const MediaSink::Id& sink_id);

  // Creates and send an issue when casting did not start because notifications
  // are disabled on the receiver device.
  void SendIssueForNotificationDisabled(const MediaSink::Id& sink_id);

  // Returns the IssueManager associated with |router_|.
  IssueManager* GetIssueManager();

  // Instantiates and initializes the issues observer.
  void StartObservingIssues();

  void OnIssue(const Issue& issue);
  void OnIssueCleared();

  // Called by |routes_observer_| when the set of active routes has changed.
  void OnRoutesUpdated(const std::vector<MediaRoute>& routes);

  // MediaSinkWithCastModesObserver:
  void OnSinksUpdated(
      const std::vector<MediaSinkWithCastModes>& sinks) override;

  // Callback passed to MediaRouter to receive response to route creation
  // requests.
  virtual void OnRouteResponseReceived(
      int route_request_id,
      const MediaSink::Id& sink_id,
      MediaCastMode cast_mode,
      const std::u16string& presentation_request_source_name,
      const RouteRequestResult& result);

  // Update the header text in the dialog model and notify observers.
  void UpdateModelHeader(const std::u16string& source_name);

  UIMediaSink ConvertToUISink(const MediaSinkWithCastModes& sink,
                              const MediaRoute* route,
                              const std::optional<Issue>& issue);

  void StopObservingMirroringMediaControllerHosts();

  // Returns the MediaRouter for this instance's BrowserContext.
  virtual MediaRouter* GetMediaRouter() const;

  const std::optional<RouteRequest> current_route_request() const {
    return current_route_request_;
  }

  MediaRouteStarter* media_route_starter() const {
    DCHECK(media_route_starter_)
        << "can't call media_route_starter() after TakeMediaRouteStarter()!";
    return media_route_starter_.get();
  }

  // Helper factory that creates both the |MediaRouteStarter| and then a
  // |MediaRouterUI| in a single step.
  static std::unique_ptr<MediaRouterUI> CreateMediaRouterUI(
      MediaRouterUIParameters params);

  raw_ptr<content::WebContentsObserver> web_contents_observer_for_test_ =
      nullptr;

  // This value is set whenever there is an outstanding issue.
  std::optional<Issue> issue_;

  // Contains up-to-date data to show in the dialog.
  CastDialogModel model_;

  // This value is set when the UI requests a route to be terminated, and gets
  // reset when the route is removed.
  std::optional<MediaRoute::Id> terminating_route_id_;

  // Observers for dialog model updates.
  // TODO(takumif): CastDialogModel should manage the observers.
  base::ObserverList<CastDialogController::Observer>::Unchecked observers_;

  // This is non-null while this instance is registered to receive
  // updates from them.
  std::unique_ptr<MediaRoutesObserver> routes_observer_;

  // This contains a value only when tracking a pending route request.
  std::optional<RouteRequest> current_route_request_;

  // Used for locale-aware sorting of sinks by name. Set during
  // Init() using the current locale.
  std::unique_ptr<icu::Collator> collator_;

  std::vector<MediaSinkWithCastModes> sinks_;
  std::vector<MediaRoute> routes_;

  std::unique_ptr<MediaRouteStarter> media_route_starter_;

  std::unique_ptr<IssuesObserver> issues_observer_;

  // Keeps track of which display the initiator WebContents is on. This is used
  // to make sure we don't show a wired display presentation over the
  // controlling window.
  std::unique_ptr<WebContentsDisplayObserver> display_observer_;

  raw_ptr<MediaRouter> router_;
  raw_ptr<LoggerImpl> logger_;

  base::OnceClosure destructor_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  // Therefore |weak_factory_| must be placed at the end.
  base::WeakPtrFactory<MediaRouterUI> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
