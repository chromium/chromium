// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_METRICS_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/media/router/media_router_metrics.h"

class Profile;

namespace media_router {

// Records UMA metrics for the behavior of a Cast dialog. A new recorder
// instance should be used each time the dialog is opened.
// TODO(takumif): Once we have removed the WebUI Cast dialog, we should move
// MediaRouter.Ui.* metrics from media_router_metrics.h into this file, and move
// this file to chrome/browser/ui/media_router/.
class CastDialogMetrics {
 public:
  // |initialization_time| is when the dialog UI started initializing. We use
  // this value as the baseline for how long the dialog took to paint, load
  // sinks, etc.
  CastDialogMetrics(const base::Time& initialization_time, Profile* profile);
  virtual ~CastDialogMetrics();

  // Records the time it took to load sinks when called for the first time. This
  // is called when the list of sinks becomes non-empty.
  void OnSinksLoaded(const base::Time& sinks_load_time);

  // Records the time it took to paint when called for the first time.
  void OnPaint(const base::Time& paint_time);

  // Records the index of the selected sink in the sink list. Also records how
  // long it took to start casting if no other action (aside from selecting a
  // sink) was taken prior to that.
  void OnStartCasting(const base::Time& start_time, int selected_sink_index);

  void OnStopCasting(bool is_local_route);

  void OnCastModeSelected();

  // Records the time it took to close the dialog, if no other action was taken
  // prior to that after opening the dialog.
  void OnCloseDialog(const base::Time& close_time);

  // Records the number of sinks, which may be 0.
  void OnRecordSinkCount(int sink_count);

 private:
  // Records the first user action if it hasn't already been recorded.
  void MaybeRecordFirstAction(MediaRouterUserAction action);

  // The time when the dialog UI started initializing.
  base::Time initialization_time_;

  // The time when the dialog was painted.
  base::Time paint_time_;

  // The time when a non-empty list of sinks was loaded.
  base::Time sinks_load_time_;

  // Whether we have already recorded the first user action taken in this dialog
  // instance.
  bool first_action_recorded_ = false;

  DISALLOW_COPY_AND_ASSIGN(CastDialogMetrics);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_METRICS_H_
