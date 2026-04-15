// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDER_H_

#include <string>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/common/aliases.h"

class GURL;

namespace record_replay {

// Builds a `Recording` from a series of actions within a tab.
//
// Owned and used by `RecordReplayManager`. It is a transient object that only
// exists during a recording session (0 or 1 per tab). It runs exclusively on
// the UI thread.
class Recorder {
 public:
  explicit Recorder(GURL start_url, base::Time start_time = base::Time::Now());
  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;
  ~Recorder();

  GURL start_url() const;
  base::Time start_time() const;

  void AddClick(Selector element_selector);
  void AddSelectChange(Selector element_selector, FieldValue value);
  void AddTextChange(Selector element_selector, FieldValue text);
  void AddAutofill(Selector element_selector,
                   Recording::Action::AutofillSpecifics::Type type,
                   std::string guid);

  void SetName(std::string name);
  void SetScreenshot(base::span<const uint8_t> screenshot);

  const Recording& recording() const { return recording_; }

 private:
  // Sets the `action`'s delta and updates `last_time_`.
  void UpdateDelta(Recording::Action& action);

  // The recording under construction.
  Recording recording_;
  // The time of the last action or, if there are no actions, the start time of
  // the recording.
  base::Time last_time_;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDER_H_
