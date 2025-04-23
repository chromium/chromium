// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_METRICS_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/capture/video_capture_types.h"

namespace permissions {
class PermissionRequest;
}

namespace media_preview_metrics {

enum class UiLocation { kPermissionPrompt, kPageInfo };
enum class PreviewType { kCamera, kMic, kCameraAndMic };
enum class PromptType { kSingle, kCombined };

struct Context {
  Context(UiLocation ui_location,
          PreviewType preview_type,
          std::optional<PromptType> prompt_type,
          base::WeakPtr<permissions::PermissionRequest>);
  ~Context();

  // This class is move- and copy-constructible:
  Context(const Context& other);
  Context(Context&& other);

  Context& operator=(const Context& other) = delete;
  Context& operator=(Context&& other) = delete;

  const UiLocation ui_location;
  const PreviewType preview_type;
  const std::optional<PromptType> prompt_type;
  const base::WeakPtr<permissions::PermissionRequest> request;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MediaPreviewDeviceSelectionUserAction {
  kNoAction = 0,
  kOpened = 1,
  kSelection = 2,
  // Add new types only immediately above this line. Remember to also update
  // tools/metrics/histograms/metadata/media/enums.xml.
  kMaxValue = kSelection,
};

void RecordPageInfoNumInUseDevices(const Context& context, int devices);
void RecordMediaPreviewDuration(const Context& context,
                                const base::TimeDelta& delta);

void RecordDeviceSelectionTotalDevices(const Context& context, int devices);
void RecordDeviceSelectionAction(
    const Context& context,
    MediaPreviewDeviceSelectionUserAction user_action);

void RecordPreviewCameraPixelHeight(const Context& context, int pixel_height);
void RecordPreviewVideoExpectedFPS(const Context& context, int expected_fps);
void RecordPreviewVideoActualFPS(const Context& context, int actual_fps);
void RecordPreviewVideoFramesRenderedPercent(const Context& context,
                                             float percent);
void RecordTotalVisiblePreviewDuration(const Context& context,
                                       const base::TimeDelta& delta);
void RecordTimeToActionWithoutPreview(const Context& context,
                                      const base::TimeDelta& delta);
void RecordPreviewDelayTime(const Context& context,
                            const base::TimeDelta& delta);

void RecordOriginTrialAllowed(UiLocation location, bool allowed);

void RecordVideoCaptureError(const Context& context,
                             media::VideoCaptureError received_error);

}  // namespace media_preview_metrics

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_METRICS_H_
