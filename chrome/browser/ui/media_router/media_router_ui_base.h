// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_BASE_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_BASE_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/presentation/presentation_service_delegate_impl.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_router_file_dialog.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "chrome/browser/ui/webui/media_router/web_contents_display_observer.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/common/media_router/media_source.h"
#include "url/gurl.h"

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

// Abstract base class for Views and WebUI implementations. Responsible for
// observing and organizing route, sink, and presentation request information,
// and executing and keeping track of route requests from the dialog to Media
// Router.
class MediaRouterUIBase
    : public QueryResultManager::Observer,
      public PresentationServiceDelegateImpl::
          DefaultPresentationRequestObserver,
      public MediaRouterFileDialog::MediaRouterFileDialogDelegate {
 public:
  MediaRouterUIBase();
  ~MediaRouterUIBase() override;

  // Initializes internal state (e.g. starts listening for MediaSinks) for
  // targeting the default MediaSource (if any) of the initiator tab that owns
  // |delegate|, as well as mirroring sources of that tab.
  // The contents of the UI will change as the default MediaSource changes.
  // If there is a default MediaSource, then PRESENTATION MediaCastMode will be
  // added to |cast_modes_|.
  // Init* methods can only be called once.
  // |initiator|: Reference to the WebContents that initiated the dialog.
  //              Must not be null.
  // |delegate|: PresentationServiceDelegateImpl of the initiator tab.
  //             Must not be null.
  // TODO(imcheng): Replace use of impl with an intermediate abstract
  // interface.
  void InitWithDefaultMediaSource(content::WebContents* initiator,
                                  PresentationServiceDelegateImpl* delegate);

  // Initializes internal state targeting the presentation specified in
  // |context|. Also sets up mirroring sources based on |initiator|.
  // This is different from InitWithDefaultMediaSource() in that it does not
  // listen for default media source changes, as the UI is fixed to the source
  // in |request|.
  // Init* methods can only be called once.
  // |initiator|: Reference to the WebContents that initiated the dialog.
  //              Must not be null.
  // |delegate|: PresentationServiceDelegateImpl of the initiator tab.
  //             Must not be null.
  // |context|: Context object for the PresentationRequest. This instance will
  //            take ownership of it. Must not be null.
  void InitWithStartPresentationContext(
      content::WebContents* initiator,
      PresentationServiceDelegateImpl* delegate,
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

  // Logs a UMA stat for the source that was cast if the result is successful.
  void MaybeReportCastingSource(MediaCastMode cast_mode,
                                const RouteRequestResult& result);

  // Returns a subset of |sinks_| that should be listed in the dialog. This
  // excludes the wired display that the initiator WebContents is on.
  virtual std::vector<MediaSinkWithCastModes> GetEnabledSinks() const;

  // Returns a source name that can be shown in the dialog.
  std::string GetTruncatedPresentationRequestSourceName() const;

  // Calls MediaRouter to add the given issue.
  void AddIssue(const IssueInfo& issue);

  // Calls MediaRouter to remove the given issue.
  void RemoveIssue(const Issue::Id& issue_id);

  // Opens a file picker for when the user selected local file casting.
  void OpenFileDialog();

  const std::vector<MediaRoute>& routes() const { return routes_; }
  content::WebContents* initiator() const { return initiator_; }

  // Used in tests for wired display presentations.
  void set_display_observer_for_test(
      std::unique_ptr<WebContentsDisplayObserver> display_observer) {
    display_observer_ = std::move(display_observer);
  }

 protected:
  struct RouteRequest {
   public:
    explicit RouteRequest(const MediaSink::Id& sink_id);
    ~RouteRequest();

    int id;
    MediaSink::Id sink_id;
  };

  static void RunRouteResponseCallbacks(
      MediaRouteResponseCallback presentation_callback,
      std::vector<MediaRouteResultCallback> callbacks,
      mojom::RoutePresentationConnectionPtr connection,
      const RouteRequestResult& result);

  std::vector<MediaSource> GetSourcesForCastMode(MediaCastMode cast_mode) const;

  // QueryResultManager::Observer:
  void OnResultsUpdated(
      const std::vector<MediaSinkWithCastModes>& sinks) override;

  // Called by |routes_observer_| when the set of active routes has changed.
  virtual void OnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids);

  // Callback passed to MediaRouter to receive response to route creation
  // requests.
  virtual void OnRouteResponseReceived(
      int route_request_id,
      const MediaSink::Id& sink_id,
      MediaCastMode cast_mode,
      const base::string16& presentation_request_source_name,
      const RouteRequestResult& result);

  // Closes the dialog after receiving a route response when using
  // |start_presentation_context_|. This prevents the dialog from trying to use
  // the same presentation request again.
  virtual void HandleCreateSessionRequestRouteResponse(
      const RouteRequestResult&);

  // Initializes the dialog with mirroring sources derived from |initiator|.
  virtual void InitCommon(content::WebContents* initiator);

  // PresentationServiceDelegateImpl::DefaultPresentationObserver
  void OnDefaultPresentationChanged(
      const content::PresentationRequest& presentation_request) override;
  void OnDefaultPresentationRemoved() override;

  // Called to update the dialog with the current list of of enabled sinks.
  virtual void UpdateSinks() = 0;

  // Populates common route-related parameters for calls to MediaRouter.
  base::Optional<RouteParameters> GetRouteParameters(
      const MediaSink::Id& sink_id,
      MediaCastMode cast_mode);

  // Returns the default PresentationRequest's frame URL if there is one.
  // Otherwise returns an empty GURL.
  GURL GetFrameURL() const;

  // Creates and sends an issue if route creation timed out.
  void SendIssueForRouteTimeout(
      MediaCastMode cast_mode,
      const base::string16& presentation_request_source_name);

  // Creates and sends an issue if casting fails for any other reason.
  void SendIssueForUnableToCast(MediaCastMode cast_mode);

  // Returns the IssueManager associated with |router_|.
  IssueManager* GetIssueManager();

  // Instantiates and initializes the issues observer.
  void StartObservingIssues();

  // MediaRouterFileDialogDelegate:
  void FileDialogSelectionFailed(const IssueInfo& issue) override;

  const base::Optional<RouteRequest> current_route_request() const {
    return current_route_request_;
  }

  StartPresentationContext* start_presentation_context() const {
    return start_presentation_context_.get();
  }

  void set_start_presentation_context_for_test(
      std::unique_ptr<StartPresentationContext> start_presentation_context) {
    start_presentation_context_ = std::move(start_presentation_context);
  }

  void set_media_router_file_dialog_for_test(
      std::unique_ptr<MediaRouterFileDialog> file_dialog) {
    media_router_file_dialog_ = std::move(file_dialog);
  }

  QueryResultManager* query_result_manager() const {
    return query_result_manager_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           UIMediaRoutesObserverAssignsCurrentCastModes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterUITest,
                           UIMediaRoutesObserverSkipsUnavailableCastModes);

  class WebContentsFullscreenOnLoadedObserver;

  // This class calls to refresh the UI when the highest priority issue is
  // updated.
  class UiIssuesObserver : public IssuesObserver {
   public:
    UiIssuesObserver(IssueManager* issue_manager, MediaRouterUIBase* ui);
    ~UiIssuesObserver() override;

    // IssuesObserver:
    void OnIssue(const Issue& issue) override;
    void OnIssuesCleared() override;

   private:
    // Reference back to the owning MediaRouterUIBase instance.
    MediaRouterUIBase* const ui_;

    DISALLOW_COPY_AND_ASSIGN(UiIssuesObserver);
  };

  class UIMediaRoutesObserver : public MediaRoutesObserver {
   public:
    using RoutesUpdatedCallback =
        base::RepeatingCallback<void(const std::vector<MediaRoute>&,
                                     const std::vector<MediaRoute::Id>&)>;
    UIMediaRoutesObserver(MediaRouter* router,
                          const MediaSource::Id& source_id,
                          const RoutesUpdatedCallback& callback);
    ~UIMediaRoutesObserver() override;

    // MediaRoutesObserver:
    void OnRoutesUpdated(
        const std::vector<MediaRoute>& routes,
        const std::vector<MediaRoute::Id>& joinable_route_ids) override;

   private:
    // Callback to the owning MediaRouterUIBase instance.
    RoutesUpdatedCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(UIMediaRoutesObserver);
  };

  // Called by |issues_observer_| when the top issue has changed.
  virtual void OnIssue(const Issue& issue) = 0;
  virtual void OnIssueCleared() = 0;

  // Populates route-related parameters for CreateRoute() when doing file
  // casting.
  base::Optional<RouteParameters> GetLocalFileRouteParameters(
      const MediaSink::Id& sink_id,
      const GURL& file_url,
      content::WebContents* tab_contents);

  // If the current URL for |web_contents| is |file_url|, requests the first
  // video in it to be shown fullscreen.
  void FullScreenFirstVideoElement(const GURL& file_url,
                                   content::WebContents* web_contents,
                                   const RouteRequestResult& result);

  // Sends a request to the file dialog to log UMA stats for the file that was
  // cast if the result is successful.
  void MaybeReportFileInformation(const RouteRequestResult& result);

  // Opens the URL in a tab, returns the tab it was opened in.
  content::WebContents* OpenTabWithUrl(const GURL& url);

  // Returns the MediaRouter for this instance's BrowserContext.
  virtual MediaRouter* GetMediaRouter() const;

  // Retrieves the browser associated with this UI.
  Browser* GetBrowser();

  // This is non-null while this instance is registered to receive
  // updates from them.
  std::unique_ptr<MediaRoutesObserver> routes_observer_;

  // This contains a value only when tracking a pending route request.
  base::Optional<RouteRequest> current_route_request_;

  // Used for locale-aware sorting of sinks by name. Set during |InitCommon()|
  // using the current locale.
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
  base::Optional<content::PresentationRequest> presentation_request_;

  // It's possible for PresentationServiceDelegateImpl to be destroyed before
  // this class.
  // (e.g. if a tab with the UI open is closed, then the tab WebContents will
  // be destroyed first momentarily before the UI WebContents).
  // Holding a WeakPtr to PresentationServiceDelegateImpl is the cleanest way to
  // handle this.
  // TODO(imcheng): hold a weak ptr to an abstract type instead.
  base::WeakPtr<PresentationServiceDelegateImpl> presentation_service_delegate_;

  // WebContents for the tab for which the Cast dialog is shown.
  content::WebContents* initiator_;

  // The dialog that handles opening the file dialog and validating and
  // returning the results.
  std::unique_ptr<MediaRouterFileDialog> media_router_file_dialog_;

  std::unique_ptr<IssuesObserver> issues_observer_;

  // Keeps track of which display the initiator WebContents is on. This is used
  // to make sure we don't show a wired display presentation over the
  // controlling window.
  std::unique_ptr<WebContentsDisplayObserver> display_observer_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  // Therefore |weak_factory_| must be placed at the end.
  base::WeakPtrFactory<MediaRouterUIBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUIBase);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_UI_BASE_H_
