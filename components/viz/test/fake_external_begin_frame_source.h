// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_FAKE_EXTERNAL_BEGIN_FRAME_SOURCE_H_
#define COMPONENTS_VIZ_TEST_FAKE_EXTERNAL_BEGIN_FRAME_SOURCE_H_

#include <set>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"

namespace base {
class TickClock;
}  // namespace base

namespace viz {

class FakeExternalBeginFrameSource : public BeginFrameSource {
 public:
  class Client {
   public:
    virtual void OnAddObserver(BeginFrameObserver* obs) = 0;
    virtual void OnRemoveObserver(BeginFrameObserver* obs) = 0;
  };

  explicit FakeExternalBeginFrameSource(double refresh_rate,
                                        bool tick_automatically);
  ~FakeExternalBeginFrameSource() override;

  void SetClient(Client* client) { client_ = client; }
  void SetPaused(bool paused);

  // BeginFrameSource implementation.
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;
  void DidFinishFrame(BeginFrameObserver* obs) override;
  void OnGpuNoLongerBusy() override {}

  BeginFrameArgs CreateBeginFrameArgs(
      BeginFrameArgs::CreationLocation location);
  BeginFrameArgs CreateBeginFrameArgs(BeginFrameArgs::CreationLocation location,
                                      const base::TickClock* now_src);
  BeginFrameArgs CreateBeginFrameArgsWithGenerator(
      base::TimeTicks frame_time,
      base::TimeTicks next_frame_time,
      base::TimeDelta vsync_interval);
  uint64_t next_begin_frame_number() const { return next_begin_frame_number_; }

  void TestOnBeginFrame(const BeginFrameArgs& args);

  size_t num_observers() const { return observers_.size(); }

  bool AllFramesDidFinish();

  using BeginFrameSource::RequestCallbackOnGpuAvailable;

 private:
  void PostTestOnBeginFrame();

  const bool tick_automatically_;
  const double milliseconds_per_frame_;
  raw_ptr<Client, DanglingUntriaged> client_ = nullptr;
  bool paused_ = false;
  BeginFrameArgs current_args_;
  uint64_t next_begin_frame_number_ = BeginFrameArgs::kStartingFrameNumber;
  std::set<raw_ptr<BeginFrameObserver, SetExperimental>> observers_;
  base::flat_map<BeginFrameObserver*, int64_t> pending_frames_;
  base::CancelableOnceClosure begin_frame_task_;
  BeginFrameSource::BeginFrameArgsGenerator begin_frame_args_generator_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FakeExternalBeginFrameSource> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_FAKE_EXTERNAL_BEGIN_FRAME_SOURCE_H_
