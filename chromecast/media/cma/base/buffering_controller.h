// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_BUFFERING_CONTROLLER_H_
#define CHROMECAST_MEDIA_CMA_BASE_BUFFERING_CONTROLLER_H_

#include <list>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace chromecast {
namespace media {
class BufferingConfig;
class BufferingState;

class BufferingController {
 public:
  typedef base::RepeatingCallback<void(bool)> BufferingNotificationCB;

  // Creates a buffering controller where the conditions to trigger rebuffering
  // are given by |config|. The whole point of the buffering controller is to
  // derive a single buffering state from the buffering state of various
  // streams.
  // |buffering_notification_cb| is a callback invoked to inform about possible
  // changes of the buffering state.
  BufferingController(
      const scoped_refptr<BufferingConfig>& config,
      const BufferingNotificationCB& buffering_notification_cb);

  BufferingController(const BufferingController&) = delete;
  BufferingController& operator=(const BufferingController&) = delete;

  ~BufferingController();

  // Creates a buffering state for one stream. This state is added to the list
  // of streams monitored by the buffering controller.
  scoped_refptr<BufferingState> AddStream(const std::string& stream_id);

  // Sets the playback time.
  void SetMediaTime(base::TimeDelta time);

  // Returns the maximum media time available for rendering.
  // Return kNoTimestamp if unknown.
  base::TimeDelta GetMaxRenderingTime() const;

  // Returns whether there is an active buffering phase.
  bool IsBuffering() const { return is_buffering_; }

  // Resets the buffering controller. This includes removing all the streams
  // that were previously added.
  void Reset();

 private:
  // Invoked each time the buffering state of one of the streams has changed.
  // If |force_notification| is set, |buffering_notification_cb_| is invoked
  // regardless whether the buffering state has changed or not.
  // If |buffering_timeout| is set, then the condition to leave the buffering
  // state is relaxed (we don't want to wait more).
  void OnBufferingStateChanged(bool force_notification,
                               bool buffering_timeout);
  void BufferingTimeoutExceeded();

  // Updates the high buffer level threshold to |high_level_threshold|
  // if needed.
  // This condition is triggered when one of the stream reached its maximum
  // capacity. In that case, to avoid possible race condition (the buffering
  // controller waits for more data to come but the buffer is to small to
  // accomodate additional data), the thresholds in |config_| are adjusted
  // accordingly.
  void UpdateHighLevelThreshold(base::TimeDelta high_level_threshold);

  // Determines the overall buffer level based on the buffer level of each
  // stream.
  bool IsHighBufferLevel();
  bool IsLowBufferLevel();

  // Logs the state of the buffering controller.
  void DumpState() const;

  base::ThreadChecker thread_checker_;

  // Settings used to determine when to start/stop buffering.
  scoped_refptr<BufferingConfig> config_;

  // Callback invoked each time there is a change of the buffering state.
  BufferingNotificationCB buffering_notification_cb_;

  // State of the buffering controller.
  bool is_buffering_;

  // Start time of a re-buffering phase.
  base::Time begin_buffering_time_;
  base::Time last_buffer_end_time_;
  bool initial_buffering_;

  bool buffering_timeout_exceeded_;
  base::OneShotTimer buffering_timer_;

  // Buffering level for each individual stream.
  typedef std::list<scoped_refptr<BufferingState> > StreamList;
  StreamList stream_list_;

  base::WeakPtr<BufferingController> weak_this_;
  base::WeakPtrFactory<BufferingController> weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_BUFFERING_CONTROLLER_H_
