// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SEQUENCE_H_
#define CHROME_BROWSER_VR_SEQUENCE_H_

#include <list>
#include "base/bind.h"
#include "base/callback.h"

namespace vr {

// This is much like an animation. It is a series of callbacks associated with
// time deltas, relative to when the sequence was queued.
class Sequence {
 public:
  Sequence();
  ~Sequence();

  void Tick(base::TimeTicks now);
  void Add(base::OnceCallback<void()> task, base::TimeDelta delta);

  bool empty() const { return tasks_.empty(); }

 private:
  struct SequencedTask {
    SequencedTask(base::OnceCallback<void()> task, base::TimeDelta delta);
    SequencedTask(SequencedTask&& other);
    ~SequencedTask();

    base::OnceCallback<void()> task;
    base::TimeDelta delta;
  };

  std::list<SequencedTask> tasks_;
  base::TimeTicks start_time_;
  bool started_ = false;

  DISALLOW_COPY_AND_ASSIGN(Sequence);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SEQUENCE_H_
