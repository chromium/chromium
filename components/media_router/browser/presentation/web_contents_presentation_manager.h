// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_WEB_CONTENTS_PRESENTATION_MANAGER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_WEB_CONTENTS_PRESENTATION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/media_router/common/mojom/media_router.mojom.h"

namespace content {
struct PresentationRequest;
class WebContents;
}  // namespace content

namespace media_router {

class MediaRoute;
class RouteRequestResult;

// Keeps track of the default PresentationRequest and presentation MediaRoutes
// associated with a WebContents. Its lifetime is tied to that of the
// WebContents.
class WebContentsPresentationManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever presentation MediaRoutes associated with the WebContents
    // are added, removed, or have their attributes changed.
    virtual void OnMediaRoutesChanged(const std::vector<MediaRoute>& routes) {}

    // |presentation_request| is a nullptr if the default PresentationRequest
    // has been removed.
    virtual void OnDefaultPresentationChanged(
        const content::PresentationRequest* presentation_request) {}

   protected:
    Observer() = default;
  };

  static base::WeakPtr<WebContentsPresentationManager> Get(
      content::WebContents* web_contents);

  // Sets the instance to be returned by Get(). If this is set, Get() ignores
  // the |web_contents| argument.
  static void SetTestInstance(WebContentsPresentationManager* test_instance);

  virtual ~WebContentsPresentationManager() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns true if there is a default presentation request for the
  // WebContents.
  virtual bool HasDefaultPresentationRequest() const = 0;

  // Gets the default presentation request for the WebContents. It is an error
  // to call this method if the default presentation request does not exist.
  virtual const content::PresentationRequest& GetDefaultPresentationRequest()
      const = 0;

  // Invoked by Media Router when a PresentationRequest is started from a
  // browser-initiated dialog. Updates the internal state and propagates the
  // request result to the presentation controller.
  virtual void OnPresentationResponse(
      const content::PresentationRequest& presentation_request,
      mojom::RoutePresentationConnectionPtr connection,
      const RouteRequestResult& result) = 0;

  virtual base::WeakPtr<WebContentsPresentationManager> GetWeakPtr() = 0;

 protected:
  WebContentsPresentationManager() = default;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_WEB_CONTENTS_PRESENTATION_MANAGER_H_
