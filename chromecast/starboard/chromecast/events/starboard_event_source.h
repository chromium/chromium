// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_CHROMECAST_EVENTS_STARBOARD_EVENT_SOURCE_H_
#define CHROMECAST_STARBOARD_CHROMECAST_EVENTS_STARBOARD_EVENT_SOURCE_H_

#include <stdint.h>

#if __has_include(<starboard/extension/graphics.h>)
#include <starboard/extension/graphics.h>
#else
#define kCobaltExtensionGraphicsName "dev.cobalt.extension.Graphics"

struct CobaltExtensionGraphicsMapToMeshColorAdjustment {
  float rgba0_scale[4];
  float rgba1_scale[4];
  float rgba2_scale[4];
  float rgba3_scale[4];
};

struct CobaltExtensionGraphicsApi {
  const char* name;
  uint32_t version;
  float (*GetMaximumFrameIntervalInMilliseconds)();
  float (*GetMinimumFrameIntervalInMilliseconds)();
  bool (*IsMapToMeshEnabled)();
  bool (*ShouldClearFrameOnShutdown)(float* clear_color_red,
                                     float* clear_color_green,
                                     float* clear_color_blue,
                                     float* clear_color_alpha);
  bool (*GetMapToMeshColorAdjustments)(
      CobaltExtensionGraphicsMapToMeshColorAdjustment* adjustment);
  bool (*GetRenderRootTransform)(float* m00,
                                 float* m01,
                                 float* m02,
                                 float* m10,
                                 float* m11,
                                 float* m12,
                                 float* m20,
                                 float* m21,
                                 float* m22);
  void (*ReportFullyDrawn)();
};
#endif

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/starboard/chromecast/events/ui_event_source.h"
#include "chromecast/starboard/chromecast/starboard_adapter/public/cast_starboard_api_adapter.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace chromecast {

// Subscribes to |SbEvent| provided by
// |CastStarboardApiAdapter| and translates them to |ui::Event| before
// dispatching them to a |ui::PlatformWindowDelegate|.
//
// This class is used both to convert starboard events to ui::Event values, and
// to filter out events that did not originate from starboard.
class StarboardEventSource : public UiEventSource {
 public:
  // Creates an event source which will dispatch events to |delegate|.
  StarboardEventSource(ui::PlatformWindowDelegate* delegate,
                       BoundsChangedCallback bounds_changed_callback,
                       TransformChangedCallback transform_changed_callback);
  ~StarboardEventSource() override;

  // UiEventSource implementation:
  bool ShouldDispatchEvent(const ui::Event& event) override;

 private:
  // Queries the Starboard graphics extension for any custom render root
  // transform.
  void CheckRenderRootTransform();

  // Receives |event| from Starboard and sends it to |context|, which represents
  // an instance of this class.
  static void SbEventHandle(void* context, const SbEvent* event);

  // Receives |event| from |SbEventHandle| and translates the event to a
  // |ui::Event|.
  void SbEventHandleInternal(const SbEvent* event);

  // Dispatches an |event| to the |delegate_| on the original |task_runner_| on
  // which |this| was created.
  void DispatchUiEvent(std::unique_ptr<ui::Event> event);

  // Creates a MouseEvent for the |input_data| at the |timestamp| with the
  // specified |ui_flags|. Stores or appends |key_flags_| based on press state.
  std::unique_ptr<ui::Event> CreateMouseEvent(const SbInputData* input_data,
                                              base::TimeTicks timestamp,
                                              int ui_flags);

  void OnWindowSizeChangedOnTaskRunner(const gfx::Size& new_size);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  raw_ptr<ui::PlatformWindowDelegate> delegate_;
  BoundsChangedCallback bounds_changed_callback_;
  TransformChangedCallback transform_changed_callback_;
  const CobaltExtensionGraphicsApi* graphics_extension_ = nullptr;
  gfx::Transform current_transform_;
  int key_flags_ = 0;

  base::WeakPtr<StarboardEventSource> weak_this_;
  base::WeakPtrFactory<StarboardEventSource> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_CHROMECAST_EVENTS_STARBOARD_EVENT_SOURCE_H_
