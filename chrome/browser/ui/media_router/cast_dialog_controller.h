// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_DIALOG_CONTROLLER_H_

#include "base/callback_forward.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"

namespace ui {
struct SelectedFileInfo;
}  // namespace ui

namespace media_router {

class CastDialogModel;

// Controller component of the Cast dialog. Responsible for handling user input,
// updating the CastDialogModel, and notifying CastDialogView of updates.
class CastDialogController {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnModelUpdated(const CastDialogModel& model) = 0;

    // Observer should drop its reference to the controller when this is called.
    virtual void OnControllerInvalidated() = 0;
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

  // Prompts the user to select a local file to cast. The callback is called
  // with the info for the selected file, or nullptr if the user declined.
  virtual void ChooseLocalFile(
      base::OnceCallback<void(const ui::SelectedFileInfo*)> callback) = 0;

  // Removes the specified issue. No-op if the ID is invalid.
  virtual void ClearIssue(const Issue::Id& issue_id) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_DIALOG_CONTROLLER_H_
