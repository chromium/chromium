// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_NOTICE_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_NOTICE_H_

#include <memory>

#include "base/timer/timer.h"

class PrefService;

namespace chromeos {
namespace quick_answers {

enum class NoticeInteractionType;

// The notice will appear up to a total of 3 times.
constexpr int kNoticeImpressionCap = 3;
// The notice will appear until viewed for a cumulative 8 seconds.
constexpr int kNoticeDurationCap = 8;

// Tracks whether quick answers notice should be shown and records
// impression count and duration when there is a interaction with the
// notice (shown, accepted and dismissed).
class QuickAnswersNotice {
 public:
  explicit QuickAnswersNotice(PrefService* prefs);

  QuickAnswersNotice(const QuickAnswersNotice&) = delete;
  QuickAnswersNotice& operator=(const QuickAnswersNotice&) = delete;

  virtual ~QuickAnswersNotice();

  // Starts showing notice. Virtual for testing.
  virtual void StartNotice();
  // Marks the notice as accepted and records the impression duration.
  // Virtual for testing.
  virtual void AcceptNotice(NoticeInteractionType interaction);
  // The notice is dismissed by users. Records the impression duration.
  // Virtual for testing.
  virtual void DismissNotice();
  // Whether the notice should be shown (based on accepted state,
  // impression count and impression duration). Virtual for testing.
  virtual bool ShouldShowNotice() const;

  // Whether users have accepted the notice.
  bool IsAccepted() const;

 private:
  // Whether the notice has been seen by users for
  // |kNoticeImpressionCap| times.
  bool HasReachedImpressionCap() const;
  // Whether the notice has been seen by users for
  // |kNoticeDurationCap| seconds.
  bool HasReachedDurationCap() const;
  // Increments the perf counter by |count|.
  void IncrementPrefCounter(const std::string& path, int count);
  // Records how long the notice has been seen by the users.
  void RecordImpressionDuration();
  // Get how many times the notice has been seen by users.
  int GetImpressionCount() const;
  // Get how long the notice has been seen by users.
  base::TimeDelta GetImpressionDuration() const;

  PrefService* const prefs_;

  // Time when the notice is shown.
  base::TimeTicks start_time_;
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_QUICK_ANSWERS_NOTICE_H_
