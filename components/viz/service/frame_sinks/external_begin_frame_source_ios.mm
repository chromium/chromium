// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/viz/service/frame_sinks/external_begin_frame_source_ios.h"

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>

#include "base/apple/mach_logging.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace {

// ProMotion devices only support up to 120Hz.
constexpr float kMaxRefreshRate = 120;

constexpr float kMinimumRefreshRate =
    (viz::BeginFrameArgs::MinInterval() == base::TimeDelta()
         ? 1
         : 1 / viz::BeginFrameArgs::MinInterval().InSecondsF());

// Translates CFTimeInterval to absolute time.
uint64_t GetMachTimeFromSeconds(CFTimeInterval seconds) {
  mach_timebase_info_data_t info;
  kern_return_t kr = mach_timebase_info(&info);
  MACH_DCHECK(kr == KERN_SUCCESS, kr) << "mach_timebase_info";
  DCHECK(info.numer);
  DCHECK(info.denom);

  // From https://developer.apple.com/library/archive/qa/qa1643/_index.html.
  base::CheckedNumeric<uint64_t> time(base::Time::kNanosecondsPerSecond *
                                      seconds);
  // Skip for fast path.
  if (info.denom != info.numer) {
    time *= info.denom;
    time /= info.numer;
  }

  if (!time.IsValid()) {
    DLOG(ERROR) << "Bailing due to overflow: "
                << base::Time::kNanosecondsPerSecond << " * " << seconds
                << " / " << info.numer << " * " << info.denom;
    return 0;
  }

  return time.ValueOrDie();
}

}  // namespace

@interface CADisplayLinkImpl : NSObject {
 @private
  // A timer object that helps to synchronize with the refresh rate of the
  // display.
  CADisplayLink* __strong _displayLink;

  // Determines if a vsync listener is enabled.
  bool _enabled;

  // A client that receives vsync updates. Owns us.
  raw_ptr<viz::ExternalBeginFrameSourceIOS> _client;

  // Current preferred refresh rate in frames per second. The system may ignore
  // this and, for example, throttle the frame rate. Please note that the frame
  // rate that the system chooses will be rounded to the nearest factor of a
  // maximum refresh rate of display. Eg, if a display supports 60Hz, the
  // refresh rate might be rounded to 15, 20, 30, and 60 FPS respectively.
  float _preferredRefreshRate;

  // The maximum refresh rate that depends on a maximum supported refresh rate
  // of a display that a device uses.
  float _maximumRefreshRate;
}

@end

@implementation CADisplayLinkImpl

- (instancetype)initWithClient:(viz::ExternalBeginFrameSourceIOS*)client {
  self = [super init];

  if (self) {
    _client = client;

    // Create a CADisplayLink and suspend it until a request for begin frames
    // comes.
    _displayLink =
        [CADisplayLink displayLinkWithTarget:self
                                    selector:@selector(displayLinkDidFire:)];
    _maximumRefreshRate = kMaxRefreshRate;
    [self setPreferredInterval:base::Hertz(_maximumRefreshRate)];
    [self setEnabled:false];
    [_displayLink addToRunLoop:NSRunLoop.currentRunLoop
                       forMode:NSRunLoopCommonModes];
  }
  return self;
}

- (id)init {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

- (void)setEnabled:(bool)enabled {
  if (!_displayLink || _enabled == enabled) {
    return;
  }

  _enabled = enabled;

  // Resume or suspend the display link's notifications.
  if (_enabled) {
    _displayLink.paused = NO;
  } else {
    _displayLink.paused = YES;
  }
}

- (void)invalidateDisplayLink {
  [self setEnabled:false];
  [_displayLink invalidate];
  _displayLink = nil;
  _client = nil;
}

- (void)setPreferredInterval:(base::TimeDelta)interval {
  if (!_displayLink) {
    return;
  }

  DCHECK_GE(interval, base::TimeDelta());
  const float refresh_rate = 1 / interval.InSecondsF();

  if (_preferredRefreshRate != refresh_rate) {
    // The preferred refresh rate mustn't exceed the maximum one. The floating
    // part can result in exceeding the maximum rate because of the division
    // operation.
    _preferredRefreshRate =
        refresh_rate > _maximumRefreshRate ? _maximumRefreshRate : refresh_rate;
    if (@available(iOS 15, *)) {
      [_displayLink
          setPreferredFrameRateRange:CAFrameRateRange{
                                         .minimum = kMinimumRefreshRate,
                                         .maximum = _maximumRefreshRate,
                                         .preferred = _preferredRefreshRate}];
    } else if (@available(iOS 10, *)) {
      [_displayLink setPreferredFramesPerSecond:_preferredRefreshRate];
    }

    // _displayLink.frameInterval is used on iOS 3-10. However, these are pretty
    // old iOS versions, which we are not targeting.
  }
}

- (void)displayLinkDidFire:(CADisplayLink*)displayLink {
  DCHECK(_client);

  // Get the previous vsync time.
  const base::TimeTicks vsync_time = base::TimeTicks::FromMachAbsoluteTime(
      GetMachTimeFromSeconds(displayLink.timestamp));

  // Get the next vsync time.
  const base::TimeTicks next_vsync_time = base::TimeTicks::FromMachAbsoluteTime(
      GetMachTimeFromSeconds(displayLink.targetTimestamp));

  // An error happened. Skip.
  if (vsync_time.is_null() || next_vsync_time.is_null()) {
    return;
  }

  // Get the interval of the current vsync.
  const base::TimeDelta vsync_interval = next_vsync_time - vsync_time;

  // If interval is not positive, we have to skip this frame.
  if (!vsync_interval.is_positive()) {
    return;
  }

  _client->OnVSync(vsync_time, next_vsync_time, vsync_interval);
}

- (int64_t)maximumRefreshRate {
  return _maximumRefreshRate;
}

@end

namespace viz {

struct ExternalBeginFrameSourceIOS::ObjCStorage {
  CADisplayLinkImpl* __strong display_link_impl;
};

ExternalBeginFrameSourceIOS::ExternalBeginFrameSourceIOS(uint32_t restart_id)
    : ExternalBeginFrameSource(this, restart_id),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->display_link_impl =
      [[CADisplayLinkImpl alloc] initWithClient:this];
}

ExternalBeginFrameSourceIOS::~ExternalBeginFrameSourceIOS() {
  // We must manually invalidate the CADisplayLink as its addToRunLoop keeps
  // strong reference to its target. Thus, releasing our wrapper won't really
  // result in destroying the object.
  [objc_storage_->display_link_impl invalidateDisplayLink];
  objc_storage_->display_link_impl = nil;
}

void ExternalBeginFrameSourceIOS::SetPreferredInterval(
    base::TimeDelta interval) {
  [objc_storage_->display_link_impl setPreferredInterval:interval];
}

base::TimeDelta ExternalBeginFrameSourceIOS::GetMaximumRefreshFrameInterval() {
  const int64_t max_refresh_rate =
      [objc_storage_->display_link_impl maximumRefreshRate];
  if (max_refresh_rate <= 0) [[unlikely]] {
    return BeginFrameArgs::DefaultInterval();
  }
  return base::Hertz(max_refresh_rate);
}

void ExternalBeginFrameSourceIOS::OnVSync(base::TimeTicks vsync_time,
                                          base::TimeTicks next_vsync_time,
                                          base::TimeDelta vsync_interval) {
  OnBeginFrame(begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), vsync_time, next_vsync_time, vsync_interval));
}

void ExternalBeginFrameSourceIOS::OnNeedsBeginFrames(bool needs_begin_frames) {
  SetEnabled(needs_begin_frames);
}

void ExternalBeginFrameSourceIOS::SetEnabled(bool enabled) {
  [objc_storage_->display_link_impl setEnabled:enabled];
}

}  // namespace viz
