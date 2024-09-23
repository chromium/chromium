// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_DIALOG_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"

namespace media_router {

class CastDialogModel;
class MediaRouteStarter;

// Controller component of the Cast dialog. Responsible for handling user input,
// updating the CastDialogModel, and notifying CastDialogView of updates.
class CastDialogController {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnModelUpdated(const CastDialogModel& model) {}

    virtual void OnCastingStarted() {}

    // Notifies observers that the observed object is being destroyed. Observers
    // MUST NOT try to destroy the observed object in response - to manage the
    // lifetime of a CastDialogController, use RegisterDestructor() below.
    virtual void OnControllerDestroying() {}
  };

  virtual ~CastDialogController() = default;

  // |observer| is notified upon registration, and whenever there is a change to
  // the dialog model.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Starts Casting to the given sink. No-op if |sink_id| is invalid or the sink
  // doesn't support |cast_mode|.
  virtual void StartCasting(const MediaSink::Id& sink_id,
                            MediaCastMode cast_mode) = 0;

  // Stops casting by terminating the route given by |route_id|. No-op if the ID
  // is invalid.
  virtual void StopCasting(const MediaRoute::Id& route_id) = 0;

  // Removes the specified issue. No-op if the ID is invalid.
  virtual void ClearIssue(const Issue::Id& issue_id) = 0;

  // Freezes and unfreezes the route given by |route_id|. No-op if the ID is
  // invalid, if the route is not currently mirroring, or if the mirroring route
  // does not support freezing.
  virtual void FreezeRoute(const MediaRoute::Id& route_id) = 0;
  virtual void UnfreezeRoute(const MediaRoute::Id& route_id) = 0;

  // Returns the MediaRouteStarter that this dialog was going to use to create
  // the mirroring or presentation routes. The dialog box is relinquishing
  // ownership, and so will be unable to start casting after this point. It's
  // intended that this API should only be used to transfer ownership to some
  // new component that will want to start casting on this dialog box's behalf.
  virtual std::unique_ptr<MediaRouteStarter> TakeMediaRouteStarter() = 0;

  // Registers a callback for when the CastDialogController has given up
  // ownership of its MediaRouteStarter and is no longer safe to use. The
  // provided closure must destroy |this| or otherwise ensure it is never used
  // again. This method can only be called once.
  //
  // TODO(crbug.com/40253570): It's awkward that CastDialogController has
  // a state where it exists but is unsafe to use, and doubly awkward that we
  // have to paper over that with this callback. Can that be fixed?
  virtual void RegisterDestructor(base::OnceClosure destructor) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_DIALOG_CONTROLLER_H_
