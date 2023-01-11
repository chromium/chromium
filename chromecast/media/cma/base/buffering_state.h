// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_BUFFERING_STATE_H_
#define CHROMECAST_MEDIA_CMA_BASE_BUFFERING_STATE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace chromecast {
namespace media {

class BufferingConfig : public base::RefCountedThreadSafe<BufferingConfig> {
 public:
  BufferingConfig(base::TimeDelta low_level_threshold,
                  base::TimeDelta high_level_threshold);

  BufferingConfig(const BufferingConfig&) = delete;
  BufferingConfig& operator=(const BufferingConfig&) = delete;

  base::TimeDelta low_level() const { return low_level_threshold_; }
  base::TimeDelta high_level() const { return high_level_threshold_; }

  void set_low_level(base::TimeDelta low_level) {
    low_level_threshold_ = low_level;
  }
  void set_high_level(base::TimeDelta high_level) {
    high_level_threshold_ = high_level;
  }

 private:
  friend class base::RefCountedThreadSafe<BufferingConfig>;
  virtual ~BufferingConfig();

  base::TimeDelta low_level_threshold_;
  base::TimeDelta high_level_threshold_;
};

class BufferingState
    : public base::RefCountedThreadSafe<BufferingState> {
 public:
  typedef base::RepeatingCallback<void(base::TimeDelta)> HighLevelBufferCB;

  enum State {
    kLowLevel,
    kMediumLevel,
    kHighLevel,
    kEosReached,
  };

  // Creates a new buffering state. The initial state is |kLowLevel|.
  // |state_changed_cb| is used to notify about possible state changes.
  // |high_level_buffer_cb| is used to adjust the high buffer threshold
  // when the underlying buffer is not large enough to accomodate
  // the current high buffer level.
  BufferingState(const std::string& stream_id,
                 const scoped_refptr<BufferingConfig>& config,
                 const base::RepeatingClosure& state_changed_cb,
                 const HighLevelBufferCB& high_level_buffer_cb);

  BufferingState(const BufferingState&) = delete;
  BufferingState& operator=(const BufferingState&) = delete;

  // Returns the buffering state.
  State GetState() const { return state_; }

  // Invoked when the buffering configuration has changed.
  // Based on the new configuration, the buffering state might change.
  // However, |state_changed_cb_| is not triggered in that case.
  void OnConfigChanged();

  // Sets the current rendering time for this stream.
  void SetMediaTime(base::TimeDelta media_time);

  // Sets/gets the maximum rendering media time for this stream.
  // The maximum rendering time is always lower than the buffered time.
  void SetMaxRenderingTime(base::TimeDelta max_rendering_time);
  base::TimeDelta GetMaxRenderingTime() const;

  // Sets the buffered time.
  void SetBufferedTime(base::TimeDelta buffered_time);

  // Notifies the buffering state that all the frames for this stream have been
  // buffered, i.e. the end of stream has been reached.
  void NotifyEos();

  // Notifies the buffering state the underlying buffer has reached
  // its maximum capacity.
  // The maximum frame timestamp in the buffer is given by |buffered_time|.
  // Note: this timestamp can be different from the one provided through
  // SetBufferedTime since SetBufferedTime takes the timestamp of a playable
  // frame which is not necessarily the case here (e.g. missing key id).
  void NotifyMaxCapacity(base::TimeDelta buffered_time);

  // Buffering state as a human readable string, for debugging.
  std::string ToString() const;

 private:
  friend class base::RefCountedThreadSafe<BufferingState>;
  virtual ~BufferingState();

  // Returns the state solely based on the buffered time.
  State GetBufferLevelState() const;

  // Updates the state to |new_state|.
  void UpdateState(State new_state);

  std::string const stream_id_;
  scoped_refptr<BufferingConfig> const config_;

  // Callback invoked each time there is a change of state.
  base::RepeatingClosure state_changed_cb_;

  // Callback invoked to adjust the high buffer level.
  HighLevelBufferCB high_level_buffer_cb_;

  // State.
  State state_;

  // Playback media time.
  // Equal to kNoTimestamp when not known.
  base::TimeDelta media_time_;

  // Maximum rendering media time.
  // This corresponds to the timestamp of the last frame sent to the hardware
  // decoder/renderer.
  base::TimeDelta max_rendering_time_;

  // Buffered media time.
  // Equal to kNoTimestamp when not known.
  base::TimeDelta buffered_time_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_BUFFERING_STATE_H_
