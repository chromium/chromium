// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// To know more about the algorithm used and the original code which this is
// based of, see
// https://wiki.corp.google.com/twiki/bin/view/Main/ChromeGoogleCodeXRef

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/speech/endpointer/energy_endpointer.h"

#include <math.h>
#include <stddef.h>

#include "base/logging.h"

namespace {

// Returns the RMS (quadratic mean) of the input signal.
float RMS(const int16_t* samples, int num_samples) {
  int64_t ssq_int64 = 0;
  int64_t sum_int64 = 0;
  for (int i = 0; i < num_samples; ++i) {
    sum_int64 += samples[i];
    ssq_int64 += samples[i] * samples[i];
  }
  // now convert to floats.
  double sum = static_cast<double>(sum_int64);
  sum /= num_samples;
  double ssq = static_cast<double>(ssq_int64);
  return static_cast<float>(sqrt((ssq / num_samples) - (sum * sum)));
}

int64_t Secs2Usecs(float seconds) {
  return static_cast<int64_t>(0.5 + (1.0e6 * seconds));
}

float GetDecibel(float value) {
  if (value > 1.0e-100) {
    return 20 * log10(value);
  }
  return -2000.0;
}

}  // namespace

namespace speech {

// Stores threshold-crossing histories for making decisions about the speech
// state.
class EnergyEndpointer::HistoryRing {
 public:
  HistoryRing() : insertion_index_(0) {}

  HistoryRing(const HistoryRing&) = delete;
  HistoryRing& operator=(const HistoryRing&) = delete;

  // Resets the ring to |size| elements each with state |initial_state|
  void SetRing(int size, bool initial_state);

  // Inserts a new entry into the ring and drops the oldest entry.
  void Insert(int64_t time_us, bool decision);

  // Returns the time in microseconds of the most recently added entry.
  int64_t EndTime() const;

  // Returns the sum of all intervals during which 'decision' is true within
  // the time in seconds specified by 'duration'. The returned interval is
  // in seconds.
  float RingSum(float duration_sec);

 private:
  struct DecisionPoint {
    int64_t time_us;
    bool decision;
  };

  std::vector<DecisionPoint> decision_points_;
  int insertion_index_;  // Index at which the next item gets added/inserted.
};

void EnergyEndpointer::HistoryRing::SetRing(int size, bool initial_state) {
  insertion_index_ = 0;
  decision_points_.clear();
  DecisionPoint init = {-1, initial_state};
  decision_points_.resize(size, init);
}

void EnergyEndpointer::HistoryRing::Insert(int64_t time_us, bool decision) {
  decision_points_[insertion_index_].time_us = time_us;
  decision_points_[insertion_index_].decision = decision;
  insertion_index_ = (insertion_index_ + 1) % decision_points_.size();
}

int64_t EnergyEndpointer::HistoryRing::EndTime() const {
  int ind = insertion_index_ - 1;
  if (ind < 0) {
    ind = decision_points_.size() - 1;
  }
  return decision_points_[ind].time_us;
}

float EnergyEndpointer::HistoryRing::RingSum(float duration_sec) {
  if (decision_points_.empty()) {
    return 0.0;
  }

  int64_t sum_us = 0;
  int ind = insertion_index_ - 1;
  if (ind < 0) {
    ind = decision_points_.size() - 1;
  }
  int64_t end_us = decision_points_[ind].time_us;
  bool is_on = decision_points_[ind].decision;
  int64_t start_us =
      end_us - static_cast<int64_t>(0.5 + (1.0e6 * duration_sec));
  if (start_us < 0) {
    start_us = 0;
  }
  size_t n_summed = 1;  // n points ==> (n-1) intervals
  while ((decision_points_[ind].time_us > start_us) &&
         (n_summed < decision_points_.size())) {
    --ind;
    if (ind < 0) {
      ind = decision_points_.size() - 1;
    }
    if (is_on) {
      sum_us += end_us - decision_points_[ind].time_us;
    }
    is_on = decision_points_[ind].decision;
    end_us = decision_points_[ind].time_us;
    n_summed++;
  }

  return 1.0e-6f * sum_us;  //  Returns total time that was super threshold.
}

EnergyEndpointer::EnergyEndpointer()
    : status_(EP_PRE_SPEECH),
      offset_confirm_dur_sec_(0),
      endpointer_time_us_(0),
      fast_update_frames_(0),
      frame_counter_(0),
      max_window_dur_(4.0),
      sample_rate_(0),
      history_(new HistoryRing()),
      decision_threshold_(0),
      estimating_environment_(false),
      noise_level_(0),
      rms_adapt_(0),
      start_lag_(0),
      end_lag_(0),
      user_input_start_time_us_(0) {}

EnergyEndpointer::~EnergyEndpointer() = default;

int EnergyEndpointer::TimeToFrame(float time) const {
  return static_cast<int32_t>(0.5 + (time / params_.frame_period()));
}

void EnergyEndpointer::Restart(bool reset_threshold) {
  status_ = EP_PRE_SPEECH;
  user_input_start_time_us_ = 0;

  if (reset_threshold) {
    decision_threshold_ = params_.decision_threshold();
    rms_adapt_ = decision_threshold_;
    noise_level_ = params_.decision_threshold() / 2.0f;
    frame_counter_ = 0;  // Used for rapid initial update of levels.
  }

  // Set up the memories to hold the history windows.
  history_->SetRing(TimeToFrame(max_window_dur_), false);

  // Flag that indicates that current input should be used for
  // estimating the environment. The user has not yet started input
  // by e.g. pressed the push-to-talk button. By default, this is
  // false for backward compatibility.
  estimating_environment_ = false;
}

void EnergyEndpointer::Init(const EnergyEndpointerParams& params) {
  params_ = params;

  // Find the longest history interval to be used, and make the ring
  // large enough to accommodate that number of frames.  NOTE: This
  // depends upon ep_frame_period being set correctly in the factory
  // that did this instantiation.
  max_window_dur_ = params_.onset_window();
  if (params_.speech_on_window() > max_window_dur_) {
    max_window_dur_ = params_.speech_on_window();
  }
  if (params_.offset_window() > max_window_dur_) {
    max_window_dur_ = params_.offset_window();
  }
  Restart(true);

  offset_confirm_dur_sec_ =
      params_.offset_window() - params_.offset_confirm_dur();
  if (offset_confirm_dur_sec_ < 0.0) {
    offset_confirm_dur_sec_ = 0.0;
  }

  user_input_start_time_us_ = 0;

  // Flag that indicates that  current input should be used for
  // estimating the environment. The user has not yet started input
  // by e.g. pressed the push-to-talk button. By default, this is
  // false for backward compatibility.
  estimating_environment_ = false;
  // The initial value of the noise and speech levels is inconsequential.
  // The level of the first frame will overwrite these values.
  noise_level_ = params_.decision_threshold() / 2.0f;
  fast_update_frames_ =
      static_cast<int64_t>(params_.fast_update_dur() / params_.frame_period());

  frame_counter_ = 0;  // Used for rapid initial update of levels.

  sample_rate_ = params_.sample_rate();
  start_lag_ =
      static_cast<int>(sample_rate_ / params_.max_fundamental_frequency());
  end_lag_ =
      static_cast<int>(sample_rate_ / params_.min_fundamental_frequency());
}

void EnergyEndpointer::StartSession() {
  Restart(true);
}

void EnergyEndpointer::EndSession() {
  status_ = EP_POST_SPEECH;
}

void EnergyEndpointer::SetEnvironmentEstimationMode() {
  Restart(true);
  estimating_environment_ = true;
}

void EnergyEndpointer::SetUserInputMode() {
  estimating_environment_ = false;
  user_input_start_time_us_ = endpointer_time_us_;
}

void EnergyEndpointer::ProcessAudioFrame(int64_t time_us,
                                         const int16_t* samples,
                                         int num_samples,
                                         float* rms_out) {
  endpointer_time_us_ = time_us;
  float rms = RMS(samples, num_samples);

  // Check that this is user input audio vs. pre-input adaptation audio.
  // Input audio starts when the user indicates start of input, by e.g.
  // pressing push-to-talk. Audio received prior to that is used to update
  // noise and speech level estimates.
  if (!estimating_environment_) {
    bool decision = false;
    if ((endpointer_time_us_ - user_input_start_time_us_) <
        Secs2Usecs(params_.contamination_rejection_period())) {
      decision = false;
      DVLOG(1) << "decision: forced to false, time: " << endpointer_time_us_;
    } else {
      decision = (rms > decision_threshold_);
    }

    history_->Insert(endpointer_time_us_, decision);

    switch (status_) {
      case EP_PRE_SPEECH:
        if (history_->RingSum(params_.onset_window()) >
            params_.onset_detect_dur()) {
          status_ = EP_POSSIBLE_ONSET;
        }
        break;

      case EP_POSSIBLE_ONSET: {
        float tsum = history_->RingSum(params_.onset_window());
        if (tsum > params_.onset_confirm_dur()) {
          status_ = EP_SPEECH_PRESENT;
        } else {  // If signal is not maintained, drop back to pre-speech.
          if (tsum <= params_.onset_detect_dur()) {
            status_ = EP_PRE_SPEECH;
          }
        }
        break;
      }

      case EP_SPEECH_PRESENT: {
        // To induce hysteresis in the state residency, we allow a
        // smaller residency time in the on_ring, than was required to
        // enter the SPEECH_PERSENT state.
        float on_time = history_->RingSum(params_.speech_on_window());
        if (on_time < params_.on_maintain_dur()) {
          status_ = EP_POSSIBLE_OFFSET;
        }
        break;
      }

      case EP_POSSIBLE_OFFSET:
        if (history_->RingSum(params_.offset_window()) <=
            offset_confirm_dur_sec_) {
          // Note that this offset time may be beyond the end
          // of the input buffer in a real-time system.  It will be up
          // to the RecognizerSession to decide what to do.
          status_ = EP_PRE_SPEECH;  // Automatically reset for next utterance.
        } else {  // If speech picks up again we allow return to SPEECH_PRESENT.
          if (history_->RingSum(params_.speech_on_window()) >=
              params_.on_maintain_dur()) {
            status_ = EP_SPEECH_PRESENT;
          }
        }
        break;

      default:
        LOG(WARNING) << "Invalid case in switch: " << status_;
        break;
    }

    // If this is a quiet, non-speech region, slowly adapt the detection
    // threshold to be about 6dB above the average RMS.
    if ((!decision) && (status_ == EP_PRE_SPEECH)) {
      decision_threshold_ = (0.98f * decision_threshold_) + (0.02f * 2 * rms);
      rms_adapt_ = decision_threshold_;
    } else {
      // If this is in a speech region, adapt the decision threshold to
      // be about 10dB below the average RMS. If the noise level is high,
      // the threshold is pushed up.
      // Adaptation up to a higher level is 5 times faster than decay to
      // a lower level.
      if ((status_ == EP_SPEECH_PRESENT) && decision) {
        if (rms_adapt_ > rms) {
          rms_adapt_ = (0.99f * rms_adapt_) + (0.01f * rms);
        } else {
          rms_adapt_ = (0.95f * rms_adapt_) + (0.05f * rms);
        }
        float target_threshold = 0.3f * rms_adapt_ + noise_level_;
        decision_threshold_ =
            (.90f * decision_threshold_) + (0.10f * target_threshold);
      }
    }

    // Set a floor
    if (decision_threshold_ < params_.min_decision_threshold()) {
      decision_threshold_ = params_.min_decision_threshold();
    }
  }

  // Update speech and noise levels.
  UpdateLevels(rms);
  ++frame_counter_;

  if (rms_out) {
    *rms_out = GetDecibel(rms);
  }
}

float EnergyEndpointer::GetNoiseLevelDb() const {
  return GetDecibel(noise_level_);
}

void EnergyEndpointer::UpdateLevels(float rms) {
  // Update quickly initially. We assume this is noise and that
  // speech is 6dB above the noise.
  if (frame_counter_ < fast_update_frames_) {
    // Alpha increases from 0 to (k-1)/k where k is the number of time
    // steps in the initial adaptation period.
    float alpha = static_cast<float>(frame_counter_) /
                  static_cast<float>(fast_update_frames_);
    noise_level_ = (alpha * noise_level_) + ((1 - alpha) * rms);
    DVLOG(1) << "FAST UPDATE, frame_counter_ " << frame_counter_
             << ", fast_update_frames_ " << fast_update_frames_;
  } else {
    // Update Noise level. The noise level adapts quickly downward, but
    // slowly upward. The noise_level_ parameter is not currently used
    // for threshold adaptation. It is used for UI feedback.
    if (noise_level_ < rms) {
      noise_level_ = (0.999f * noise_level_) + (0.001f * rms);
    } else {
      noise_level_ = (0.95f * noise_level_) + (0.05f * rms);
    }
  }
  if (estimating_environment_ || (frame_counter_ < fast_update_frames_)) {
    decision_threshold_ = noise_level_ * 2;  // 6dB above noise level.
    // Set a floor
    if (decision_threshold_ < params_.min_decision_threshold()) {
      decision_threshold_ = params_.min_decision_threshold();
    }
  }
}

EpStatus EnergyEndpointer::Status(int64_t* status_time) const {
  *status_time = history_->EndTime();
  return status_;
}

}  // namespace speech
