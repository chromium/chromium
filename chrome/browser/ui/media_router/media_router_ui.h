// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes_observer.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "chrome/browser/ui/webui/media_router/web_contents_display_observer.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/media_source.h"
#include "url/origin.h"

class Browser;
class GURL;

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

// Functions as an intermediary between MediaRouter and Views Cast dialog.
class MediaRouterUI : public CastDialogController,
                      public MediaSinkWithCastModesObserver,
                      public WebContentsPresentationManager::Observer {
 public:
  struct RouteRequest {
   public:
    explicit RouteRequest(const MediaSink::Id& sink_id);
    ~RouteRequest();

    int id;
    MediaSink::Id sink_id;
  };

  explicit MediaRouterUI(content::WebContents* initiator);

  MediaRouterUI(const MediaRouterUI&) = delete;
  MediaRouterUI& operator=(const MediaRouterUI&) = delete;

  ~MediaRouterUI() override;

  // CastDialogController:
  void AddObserver(CastDialogController::Observer* observer) override;
  void RemoveObserver(CastDialogController::Observer* observer) override;
  void StartCasting(const std::string& sink_id,
                    MediaCastMode cast_mode) override;
  void StopCasting(const std::string& route_id) override;
  void ClearIssue(const Issue::Id& issue_id) override;
  content::WebContents* GetInitiator() override;
  std::unique_ptr<StartPresentationContext> TakeStartPresentationContext()
      override;

  // Initializes internal state (e.g. starts listening for MediaSinks) for
  // targeting the default MediaSource (if any) of |initiator_|. The contents of
  // the UI will change as the default MediaSource changes. If there is a
  // default MediaSource, then PRESENTATION MediaCastMode will be added to
  // |cast_modes_|. Init* methods can only be called once.
  void InitWithDefaultMediaSource();
  // Initializes mirroring sources of the tab in addition to what is done by
  // |InitWithDefaultMediaSource()|.
  void InitWithDefaultMediaSourceAndMirroring();

  // Initializes internal state targeting the presentation specified in
  // |context|. This is different from InitWithDefaultMediaSource*() in that it
  // does not listen for default media source changes, as the UI is fixed to the
  // source in |context|. Init* methods can only be called once.
  // |context|: Context object for the PresentationRequest. This instance will
  // take ownership of it. Must not be null.
  void InitWithStartPresentationContext(
      std::unique_ptr<StartPresentationContext> context);
  // Initializes mirroring sources of the tab in addition to what is done by
  // |InitWithStartPresentationContext()|.
  void InitWithStartPresentationContextAndMirroring(
      std::unique_ptr<StartPresentationContext> context);

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

  // Returns a PresentationRequest source name that can be shown in the dialog.
  std::u16string GetPresentationRequestSourceName() const;

  // Calls MediaRouter to add the given issue.
  void AddIssue(const IssueInfo& issue);

  // Calls MediaRouter to remove the given issue.
  void RemoveIssue(const Issue::Id& issue_id);

  // Uses LoggerImpl to log current available sinks.
  void LogMediaSinkStatus();

  const std::vector<MediaRoute>& routes() const { return routes_; }
  content::WebContents* initiator() const { return initiator_; }

  void SimulateDocumentAvailableForTest();

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
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest,
                           RouteCreationTimeoutForPresentation);
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

  std::vector<MediaSource> GetSourcesForCastMode(MediaCastMode cast_mode) const;

  // Closes the dialog after receiving a route response when using
  // |start_presentation_context_|. This prevents the dialog from trying to use
  // the same presentation request again.
  virtual void HandleCreateSessionRequestRouteResponse(
      const RouteRequestResult&);

  // Initializes the dialog with mirroring sources derived from |initiator_|.
  virtual void InitCommon();
  void InitMirroring();

  // WebContentsPresentationManager::Observer
  void OnDefaultPresentationChanged(
      const content::PresentationRequest* presentation_request) override;

  void OnDefaultPresentationRemoved();

  // Called to update the dialog with the current list of of enabled sinks.
  void UpdateSinks();

  // Populates common route-related parameters for calls to MediaRouter.
  absl::optional<RouteParameters> GetRouteParameters(
      const MediaSink::Id& sink_id,
      MediaCastMode cast_mode);

  // Returns the default PresentationRequest's frame origin if there is one.
  // Otherwise returns an opaque origin.
  url::Origin GetFrameOrigin() const;

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
  void UpdateModelHeader();

  UIMediaSink ConvertToUISink(const MediaSinkWithCastModes& sink,
                              const MediaRoute* route,
                              const absl::optional<Issue>& issue);

  // Opens the URL in a tab, returns the tab it was opened in.
  content::WebContents* OpenTabWithUrl(const GURL& url);

  // Returns the MediaRouter for this instance's BrowserContext.
  virtual MediaRouter* GetMediaRouter() const;

  // Retrieves the browser associated with this UI.
  Browser* GetBrowser();

  const absl::optional<RouteRequest> current_route_request() const {
    return current_route_request_;
  }

  StartPresentationContext* start_presentation_context() const {
    return start_presentation_context_.get();
  }

  QueryResultManager* query_result_manager() const {
    return query_result_manager_.get();
  }

  void set_start_presentation_context_for_test(
      std::unique_ptr<StartPresentationContext> start_presentation_context) {
    start_presentation_context_ = std::move(start_presentation_context);
  }

  raw_ptr<content::WebContentsObserver> web_contents_observer_for_test_ =
      nullptr;

  // This value is set whenever there is an outstanding issue.
  absl::optional<Issue> issue_;

  // Contains up-to-date data to show in the dialog.
  CastDialogModel model_;

  // This value is set when the UI requests a route to be terminated, and gets
  // reset when the route is removed.
  absl::optional<MediaRoute::Id> terminating_route_id_;

  // Observers for dialog model updates.
  // TODO(takumif): CastDialogModel should manage the observers.
  base::ObserverList<CastDialogController::Observer>::Unchecked observers_;

  // This is non-null while this instance is registered to receive
  // updates from them.
  std::unique_ptr<MediaRoutesObserver> routes_observer_;

  // This contains a value only when tracking a pending route request.
  absl::optional<RouteRequest> current_route_request_;

  // Used for locale-aware sorting of sinks by name. Set during
  // InitCommon() using the current locale.
  std::unique_ptr<icu::Collator> collator_;

  std::vector<MediaSinkWithCastModes> sinks_;
  std::vector<MediaRoute> routes_;

  // Monitors and reports sink availability.
  std::unique_ptr<QueryResultManager> query_result_manager_;

  // If set, then the result of the next presentation route request will
  // be handled by this object.
  std::unique_ptr<StartPresentationContext> start_presentation_context_;

  // Set to the presentation request corresponding to the presentation cast
  // mode, if supported. Otherwise set to nullopt.
  absl::optional<content::PresentationRequest> presentation_request_;

  // |presentation_manager_| notifies |this| whenever there is an update to the
  // default PresentationRequest or MediaRoutes associated with |initiator_|.
  base::WeakPtr<WebContentsPresentationManager> presentation_manager_;

  // WebContents for the tab for which the Cast dialog is shown.
  const raw_ptr<content::WebContents> initiator_;

  std::unique_ptr<IssuesObserver> issues_observer_;

  // Keeps track of which display the initiator WebContents is on. This is used
  // to make sure we don't show a wired display presentation over the
  // controlling window.
  std::unique_ptr<WebContentsDisplayObserver> display_observer_;

  raw_ptr<LoggerImpl> logger_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  // Therefore |weak_factory_| must be placed at the end.
  base::WeakPtrFactory<MediaRouterUI> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_H_
