// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTE_STARTER_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTE_STARTER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes_observer.h"
#include "chrome/browser/ui/media_router/presentation_request_source_observer.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/common/issue.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
struct PresentationRequest;
class WebContents;
}  // namespace content

namespace media_router {
class MediaRouter;
class QueryResultManager;

// Provides cast services (lists of sinks, routes, start & terminate route) to
// UI controllers
class MediaRouteStarter : public WebContentsPresentationManager::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnPresentationRequestSourceUpdated(
        std::u16string presentation_request_source_name) = 0;
  };

  MediaRouteStarter(
      const CastModeSet& initial_modes,
      content::WebContents* web_contents,
      std::unique_ptr<StartPresentationContext> start_presentation_context);
  MediaRouteStarter(const MediaRouteStarter&) = delete;
  MediaRouteStarter& operator=(const MediaRouteStarter&) = delete;

  ~MediaRouteStarter() override;

  // Adds/removes an observer that is notified when the presentation request
  // source is updated.
  void AddPresentationRequestSourceObserver(
      PresentationRequestSourceObserver* observer);
  void RemovePresentationRequestSourceObserver(
      PresentationRequestSourceObserver* observer);

  void AddMediaSinkWithCastModesObserver(
      MediaSinkWithCastModesObserver* observer);
  void RemoveMediaSinkWithCastModesObserver(
      MediaSinkWithCastModesObserver* observer);

  content::WebContents* GetWebContents() const { return web_contents_; }

  // Returns the profile associated with this casting attempt. Will either be
  // the profile associated with the browser containing the contentg being
  // cast, or the active user profile in the case of desktop mirroring.
  virtual Profile* GetProfile() const;

  // Returns the MediaRouter for this instance's BrowserContext.
  virtual MediaRouter* GetMediaRouter() const;

  // Sets the component used for media router logging statements made by the
  // MediaRouteStarter
  void SetLoggerComponent(const std::string& component) {
    component_ = component;
  }

  // Constructs |RouteParameters| with the necessary information for
  // |StartRoute| to construct a route between |sink_id| and an available source
  // that supports |cast_mode|. The returned |RouteParameters| can be modified
  // by the caller as desired, notably any callbacks that the caller wishes to
  // have called on route creation shuold be added to
  // |RouteParameters.route_result_callbacks|.
  std::unique_ptr<RouteParameters> CreateRouteParameters(
      const MediaSink::Id& sink_id,
      MediaCastMode cast_mode);

  void StartRoute(std::unique_ptr<RouteParameters> params);

  // Returns a PresentationRequest source name that can be shown in the dialog.
  std::u16string GetPresentationRequestSourceName() const;

  // Determines if the given cast mode requires user permission, and if so,
  // obtains it. Will only be false if permission is required and user does not
  // provide.
  static bool GetScreenCapturePermission(MediaCastMode cast_mode);

 private:
  friend class MediaRouteStarterTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouteStarterTest,
                           OnPresentationRequestSourceRemoved);
  FRIEND_TEST_ALL_PREFIXES(MediaRouteStarterTest,
                           OnPresentationRequestSourceUpdated);
  FRIEND_TEST_ALL_PREFIXES(MediaRouteStarterTest, GetScreenCapturePermission);

  void InitPresentationSources(const CastModeSet& initial_modes);
  void InitMirroringSources(const CastModeSet& initial_modes);

  content::BrowserContext* GetBrowserContext() const;

  // Returns the default PresentationRequest's frame origin if there is one.
  // Otherwise returns an opaque origin.
  url::Origin GetFrameOrigin() const;

  // Returns the QueryResultManager for this instance's |GetMediaRouter|
  QueryResultManager* GetQueryResultManager() const {
    return query_result_manager_.get();
  }

  // Returns true if the specified cast mode is among the cast modes specified
  // as supported for the service when initialized.
  static bool IsCastModeAvailable(const CastModeSet& modes, MediaCastMode mode);

  // WebContentsPresentationManager::Observer
  void OnDefaultPresentationChanged(
      const content::PresentationRequest* presentation_request) override;

  // Component name used for media router logging.
  std::string component_ = "MediaRouteStarter";

  // If set, this is the tab for which this casting request was initiated. May
  // be null in the case of desktop tab casting.
  const raw_ptr<content::WebContents> web_contents_;

  // If set, then the result of the next presentation route request will
  // be handled by this object instead of |presentation_manager_|
  std::unique_ptr<StartPresentationContext> start_presentation_context_;

  // |presentation_manager_| notifies |this| whenever there is an update to the
  // default PresentationRequest or MediaRoutes associated with |web_contents_|.
  base::WeakPtr<WebContentsPresentationManager> presentation_manager_;

  // Monitors and reports sink availability.
  std::unique_ptr<QueryResultManager> query_result_manager_;

  // Set to the presentation request corresponding to the presentation cast
  // mode, if supported. Otherwise set to nullopt.
  absl::optional<content::PresentationRequest> presentation_request_;

  // Registered observers.
  base::ObserverList<PresentationRequestSourceObserver>::Unchecked observers_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  // Therefore |weak_factory_| must be placed at the end.
  base::WeakPtrFactory<MediaRouteStarter> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTE_STARTER_H_
