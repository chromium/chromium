// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_BEGIN_FRAME_ARGS_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_BEGIN_FRAME_ARGS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/time/time.h"
#include "components/viz/common/viz_common_export.h"

namespace perfetto {
class EventContext;
namespace protos {
namespace pbzero {
class BeginFrameArgsV2;
}
}  // namespace protos
}  // namespace perfetto

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
class TracedValue;
}  // namespace trace_event
}  // namespace base

/**
 * In debug builds we trace the creation origin of BeginFrameArgs objects. We
 * reuse the base::Location system to do that.
 *
 * However, in release builds we don't want this as it doubles the size of the
 * BeginFrameArgs object. As well it adds a number of largish strings to the
 * binary. Despite the argument being unused, most compilers are unable to
 * optimise it away even when unused. Instead we use the BEGINFRAME_FROM_HERE
 * macro to prevent the data even getting referenced.
 */
#ifdef NDEBUG
#define BEGINFRAME_FROM_HERE nullptr
#else
#define BEGINFRAME_FROM_HERE FROM_HERE
#endif

namespace viz {

struct VIZ_COMMON_EXPORT BeginFrameId {
  // |source_id| and |sequence_number| identify a BeginFrame. These are set by
  // the original BeginFrameSource that created the BeginFrameArgs. When
  // |source_id| of consecutive BeginFrameArgs changes, observers should expect
  // the continuity of |sequence_number| to break.
  uint64_t source_id = 0;
  uint64_t sequence_number = 0;

  // Creates an invalid set of values.
  BeginFrameId();
  BeginFrameId(const BeginFrameId& id);
  BeginFrameId& operator=(const BeginFrameId& id);
  BeginFrameId(uint64_t source_id, uint64_t sequence_number);

  friend std::strong_ordering operator<=>(const BeginFrameId&,
                                          const BeginFrameId&) = default;

  bool IsNextInSequenceTo(const BeginFrameId& previous) const;
  bool IsSequenceValid() const;
  std::string ToString() const;
};

struct VIZ_COMMON_EXPORT PossibleDeadline {
  PossibleDeadline(int64_t vsync_id,
                   base::TimeDelta latch_delta,
                   base::TimeDelta present_delta);
  ~PossibleDeadline();

  // Out-of-line copy and assignment operators.
  PossibleDeadline(const PossibleDeadline& other);
  PossibleDeadline(PossibleDeadline&& other);
  PossibleDeadline& operator=(const PossibleDeadline& other);
  PossibleDeadline& operator=(PossibleDeadline&& other);

  // Passed during swap to select the deadline.
  int64_t vsync_id;
  // Time delta from `BeginFrameArgs::frame_time` when the receiving pipeline
  // stage starts to do its work. All viz CPU and GPU work need to be complete
  // by this time to not miss a frame.
  base::TimeDelta latch_delta;
  // Time delta from `BeginFrameArgs::frame_time` when the frame is expected to
  // be presented to the user. This would be the present time if viz finished
  // its work before `latch_delta` and subsequent stages were also on time.
  base::TimeDelta present_delta;
};

struct VIZ_COMMON_EXPORT PossibleDeadlines {
  explicit PossibleDeadlines(size_t preferred_index);
  ~PossibleDeadlines();

  // Out-of-line copy and assignment operators.
  PossibleDeadlines(const PossibleDeadlines& other);
  PossibleDeadlines(PossibleDeadlines&& other);
  PossibleDeadlines& operator=(const PossibleDeadlines& other);
  PossibleDeadlines& operator=(PossibleDeadlines&& other);

  const PossibleDeadline& GetPreferredDeadline() const;

  // Index into to `deadlines` vector picked by the OS as the default.
  size_t preferred_index;
  std::vector<PossibleDeadline> deadlines;
};

struct VIZ_COMMON_EXPORT BeginFrameArgs {
  enum BeginFrameArgsType {
    INVALID,
    NORMAL,
    MISSED,
  };
  static const char* TypeToString(BeginFrameArgsType type);

  static constexpr uint64_t kStartingSourceId = 0;
  // |source_id| for BeginFrameArgs not created by a BeginFrameSource. Used to
  // avoid sequence number conflicts of BeginFrameArgs manually fed to an
  // observer with those fed to the observer by the its BeginFrameSource.
  static constexpr uint64_t kManualSourceId = UINT32_MAX;

  static constexpr uint64_t kInvalidFrameNumber = 0;
  static constexpr uint64_t kStartingFrameNumber = 1;

  // Creates an invalid set of values.
  BeginFrameArgs();
  ~BeginFrameArgs();

  BeginFrameArgs(const BeginFrameArgs& args);
  BeginFrameArgs& operator=(const BeginFrameArgs& args);

#ifdef NDEBUG
  typedef const void* CreationLocation;
#else
  typedef const base::Location& CreationLocation;
  base::Location created_from;
#endif

  // You should be able to find all instances where a BeginFrame has been
  // created by searching for "BeginFrameArgs::Create".
  // The location argument should **always** be BEGINFRAME_FROM_HERE macro.
  static BeginFrameArgs Create(CreationLocation location,
                               uint64_t source_id,
                               uint64_t sequence_number,
                               base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval,
                               BeginFrameArgsType type);

  // This is the default interval assuming 60Hz to use to avoid sprinkling the
  // code with magic numbers.
  static constexpr base::TimeDelta DefaultInterval() {
    return base::Seconds(1) / 60;
  }

  // This is the preferred interval to use when the producer can animate at the
  // max interval supported by the Display.
  static constexpr base::TimeDelta MinInterval() { return base::Seconds(0); }

  // This is a hard-coded deadline adjustment used by the display compositor.
  // Using 1/3 of the vsync as the default adjustment gives the display
  // compositor the last 1/3 of a frame to produce output, the client impl
  // thread the middle 1/3 of a frame to produce output, and the client's main
  // thread the first 1/3 of a frame to produce output.
  static constexpr float kDefaultEstimatedDisplayDrawTimeRatio = 1.f / 3;

  // Returns how much time the display should reserve for draw and swap if the
  // BeginFrame interval is |interval|.
  static base::TimeDelta DefaultEstimatedDisplayDrawTime(
      base::TimeDelta interval) {
    return interval * kDefaultEstimatedDisplayDrawTimeRatio;
  }

  bool IsValid() const { return interval >= base::TimeDelta(); }

  // TODO(nuskos): Once we have a protozero -> String utility function remove
  // these base::trace_event json dictionary functions.
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;
  void AsValueInto(base::trace_event::TracedValue* dict) const;
  void AsProtozeroInto(perfetto::EventContext& ctx,
                       perfetto::protos::pbzero::BeginFrameArgsV2* args) const;

  std::string ToString() const;

  // The time at which the frame started. Used, for example, by animations to
  // decide to slow down or skip ahead.
  base::TimeTicks frame_time;
  // The time by which the receiving pipeline stage should do its work.
  base::TimeTicks deadline;
  // The inverse of the desired frame rate.
  base::TimeDelta interval;

  BeginFrameId frame_id;

  // |trace_id| is used as the id for the trace-events associated with this
  // begin-frame. The trace-id is set by the service, and can be used by both
  // the client and service as the id for trace-events.
  int64_t trace_id = -1;

  // The time when viz dispatched this to a client.
  base::TimeTicks dispatch_time;
  // For clients to denote when they received this being dispatched.
  base::TimeTicks client_arrival_time;

  BeginFrameArgsType type = INVALID;
  bool on_critical_path = true;

  // If true, observers of this BeginFrame should not produce a new
  // CompositorFrame, but instead only run the (web-visible) side effects of the
  // BeginFrame, such as updating animations and layout. Such a BeginFrame
  // effectively advances an observer's view of frame time, which in turn may
  // trigger side effects such as loading of new resources.
  //
  // Observers have to explicitly opt-in to receiving animate_only
  // BeginFrames via BeginFrameObserver::WantsAnimateOnlyBeginFrames.
  //
  // Designed for use in headless, in conjunction with
  // --disable-threaded-animation, --disable-threaded-scrolling, and
  // --disable-checker-imaging, see bit.ly/headless-rendering.
  bool animate_only = false;

  // Number of frames being skipped during throttling since last BeginFrame
  // sent.
  uint64_t frames_throttled_since_last = 0;

  // This is not serialized for mojo as it should only be used internal to viz.
  // Note `deadline` is not yet updated to one of these deadline since some
  // code still assumes `deadline` is a multiple of `interval` from
  // `frame_time`.
  std::optional<PossibleDeadlines> possible_deadlines;

 private:
  BeginFrameArgs(uint64_t source_id,
                 uint64_t sequence_number,
                 base::TimeTicks frame_time,
                 base::TimeTicks deadline,
                 base::TimeDelta interval,
                 BeginFrameArgsType type);
};

// Sent by a BeginFrameObserver as acknowledgment of completing a BeginFrame.
struct VIZ_COMMON_EXPORT BeginFrameAck {
  BeginFrameAck() = default;

  // Constructs an instance as a response to the specified BeginFrameArgs.
  BeginFrameAck(const BeginFrameArgs& args, bool has_damage);

  BeginFrameAck(uint64_t source_id,
                uint64_t sequence_number,
                bool has_damage,
                int64_t trace_id = -1);

  BeginFrameAck(const BeginFrameAck& other) = default;
  BeginFrameAck& operator=(const BeginFrameAck& other) = default;

  // Creates a BeginFrameAck for a manual BeginFrame. Used when clients produce
  // a CompositorFrame without prior BeginFrame, e.g. for synchronous drawing.
  static BeginFrameAck CreateManualAckWithDamage();

  // Source identifier and Sequence number of the BeginFrame that is
  // acknowledged. The BeginFrameSource that receives the acknowledgment uses
  // this to discard BeginFrameAcks for BeginFrames sent by a different source.
  // Such a situation may occur when the BeginFrameSource of the observer
  // changes while a BeginFrame from the old source is still in flight.
  BeginFrameId frame_id;

  // The |trace_id| of the BeginFrame that is acknowledged.
  int64_t trace_id = -1;

  // |true| if the observer has produced damage (e.g. sent a CompositorFrame or
  // damaged a surface) as part of responding to the BeginFrame.
  bool has_damage = false;

  // Specifies the interval at which the client's content is updated. This can
  // be used to configure the display to the optimal vsync interval available.
  // If unspecified, or set to BeginFrameArgs::MinInterval, it is assumed that
  // the client can animate at the maximum frame rate supported by the Display.
  std::optional<base::TimeDelta> preferred_frame_interval;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_BEGIN_FRAME_ARGS_H_
