// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CAMERA_APP_UI_RESOURCES_H_
#define CHROMEOS_COMPONENTS_CAMERA_APP_UI_RESOURCES_H_

#include "chromeos/components/camera_app_ui/resources/strings/grit/chromeos_camera_app_strings.h"
#include "chromeos/grit/chromeos_camera_app_resources.h"
#include "chromeos/grit/chromeos_camera_app_resources_map.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"

namespace chromeos {

// TODO(crbug.com/980846): Use camelCase for name instead after totally
// migrating to SWA.
const struct {
  const char* name;
  int id;
} kStringResourceMap[] = {
    {"label_timer_10s", IDS_LABEL_TIMER_10S},
    {"help_button", IDS_HELP_BUTTON},
    {"dialog_cancel_button", IDS_DIALOG_CANCEL_BUTTON},
    {"print_button", IDS_PRINT_BUTTON},
    {"gallery_button", IDS_GALLERY_BUTTON},
    {"error_msg_save_file_failed", IDS_ERROR_MSG_SAVE_FILE_FAILED},
    {"export_button", IDS_EXPORT_BUTTON},
    {"toggle_timer_button", IDS_TOGGLE_TIMER_BUTTON},
    {"migrate_pictures_msg", IDS_MIGRATE_PICTURES_MSG},
    {"aria_grid_3x3", IDS_ARIA_GRID_3X3},
    {"record_video_start_button", IDS_RECORD_VIDEO_START_BUTTON},
    {"record_video_stop_button", IDS_RECORD_VIDEO_STOP_BUTTON},
    {"switch_camera_button", IDS_SWITCH_CAMERA_BUTTON},
    {"timer_duration_button", IDS_TIMER_DURATION_BUTTON},
    {"label_timer_3s", IDS_LABEL_TIMER_3S},
    {"status_msg_recording_stopped", IDS_STATUS_MSG_RECORDING_STOPPED},
    {"aria_grid_4x4", IDS_ARIA_GRID_4X4},
    {"label_grid_4x4", IDS_LABEL_GRID_4X4},
    {"camera_resolution_button", IDS_CAMERA_RESOLUTION_BUTTON},
    {"photo_resolution_button", IDS_PHOTO_RESOLUTION_BUTTON},
    {"video_resolution_button", IDS_VIDEO_RESOLUTION_BUTTON},
    {"label_front_camera", IDS_LABEL_FRONT_CAMERA},
    {"label_back_camera", IDS_LABEL_BACK_CAMERA},
    {"label_external_camera", IDS_LABEL_EXTERNAL_CAMERA},
    {"label_photo_resolution", IDS_LABEL_PHOTO_RESOLUTION},
    {"label_detail_photo_resolution", IDS_LABEL_DETAIL_PHOTO_RESOLUTION},
    {"label_video_resolution", IDS_LABEL_VIDEO_RESOLUTION},
    {"delete_confirmation_msg", IDS_DELETE_CONFIRMATION_MSG},
    {"switch_take_photo_button", IDS_SWITCH_TAKE_PHOTO_BUTTON},
    {"label_switch_take_photo_button", IDS_LABEL_SWITCH_TAKE_PHOTO_BUTTON},
    {"toggle_grid_button", IDS_TOGGLE_GRID_BUTTON},
    {"take_photo_button", IDS_TAKE_PHOTO_BUTTON},
    {"error_msg_no_camera", IDS_ERROR_MSG_NO_CAMERA},
    {"error_msg_record_start_failed", IDS_ERROR_MSG_RECORD_START_FAILED},
    {"toggle_mic_button", IDS_TOGGLE_MIC_BUTTON},
    {"expert_mode_button", IDS_EXPERT_MODE_BUTTON},
    {"expert_preview_metadata", IDS_EXPERT_PREVIEW_METADATA},
    {"expert_save_metadata", IDS_EXPERT_SAVE_METADATA},
    {"expert_print_performance_logs", IDS_EXPERT_PRINT_PERFORMANCE_LOGS},
    {"error_msg_expert_mode_not_supported",
     IDS_ERROR_MSG_EXPERT_MODE_NOT_SUPPORTED},
    {"feedback_button", IDS_FEEDBACK_BUTTON},
    {"error_msg_take_photo_failed", IDS_ERROR_MSG_TAKE_PHOTO_FAILED},
    {"error_msg_take_portrait_photo_failed",
     IDS_ERROR_MSG_TAKE_PORTRAIT_PHOTO_FAILED},
    {"description", IDS_DESCRIPTION},
    {"label_grid_3x3", IDS_LABEL_GRID_3X3},
    {"take_photo_cancel_button", IDS_TAKE_PHOTO_CANCEL_BUTTON},
    {"delete_button", IDS_DELETE_BUTTON},
    {"error_msg_empty_recording", IDS_ERROR_MSG_EMPTY_RECORDING},
    {"delete_multi_confirmation_msg", IDS_DELETE_MULTI_CONFIRMATION_MSG},
    {"gallery_images", IDS_GALLERY_IMAGES},
    {"error_msg_gallery_export_failed", IDS_ERROR_MSG_GALLERY_EXPORT_FAILED},
    {"status_msg_camera_switched", IDS_STATUS_MSG_CAMERA_SWITCHED},
    {"name", IDS_NAME},
    {"error_msg_file_system_failed", IDS_ERROR_MSG_FILE_SYSTEM_FAILED},
    {"settings_button", IDS_SETTINGS_BUTTON},
    {"dialog_ok_button", IDS_DIALOG_OK_BUTTON},
    {"label_grid_golden", IDS_LABEL_GRID_GOLDEN},
    {"switch_record_video_button", IDS_SWITCH_RECORD_VIDEO_BUTTON},
    {"label_switch_record_video_button", IDS_LABEL_SWITCH_RECORD_VIDEO_BUTTON},
    {"toggle_mirror_button", IDS_TOGGLE_MIRROR_BUTTON},
    {"grid_type_button", IDS_GRID_TYPE_BUTTON},
    {"label_30fps", IDS_LABEL_30FPS},
    {"label_60fps", IDS_LABEL_60FPS},
    {"toggle_60fps_button", IDS_TOGGLE_60FPS_BUTTON},
    {"back_button", IDS_BACK_BUTTON},
    {"switch_take_square_photo_button", IDS_SWITCH_TAKE_SQUARE_PHOTO_BUTTON},
    {"label_switch_take_square_photo_button",
     IDS_LABEL_SWITCH_TAKE_SQUARE_PHOTO_BUTTON},
    {"switch_take_portrait_photo_button",
     IDS_SWITCH_TAKE_PORTRAIT_PHOTO_BUTTON},
    {"label_switch_take_portrait_photo_button",
     IDS_LABEL_SWITCH_TAKE_PORTRAIT_PHOTO_BUTTON},
    {"confirm_review_button", IDS_CONFIRM_REVIEW_BUTTON},
    {"cancel_review_button", IDS_CANCEL_REVIEW_BUTTON},
    {"play_result_video_button", IDS_PLAY_RESULT_VIDEO_BUTTON},
    {"record_video_paused_msg", IDS_RECORD_VIDEO_PAUSED_MSG},
    {"take_video_snapshot_button", IDS_TAKE_VIDEO_SNAPSHOT_BUTTON},
    {"record_video_pause_button", IDS_RECORD_VIDEO_PAUSE_BUTTON},
    {"record_video_resume_button", IDS_RECORD_VIDEO_RESUME_BUTTON},
    {"feedback_description_placeholder", IDS_FEEDBACK_DESCRIPTION_PLACEHOLDER},
};

const struct {
  const char* path;
  int id;
} kGritResourceMap[] = {
    {"js/browser_proxy/browser_proxy.js",
     IDR_CAMERA_WEBUI_BROWSER_PROXY_JS},
    {"js/mojo/camera_intent.mojom-lite.js",
     IDR_CAMERA_CAMERA_INTENT_MOJOM_LITE_JS},
    {"js/mojo/image_capture.mojom-lite.js",
     IDR_CAMERA_IMAGE_CAPTURE_MOJOM_LITE_JS},
    {"js/mojo/camera_common.mojom-lite.js",
     IDR_CAMERA_CAMERA_COMMON_MOJOM_LITE_JS},
    {"js/mojo/camera_metadata.mojom-lite.js",
     IDR_CAMERA_CAMERA_METADATA_MOJOM_LITE_JS},
    {"js/mojo/camera_metadata_tags.mojom-lite.js",
     IDR_CAMERA_CAMERA_METADATA_TAGS_MOJOM_LITE_JS},
    {"js/mojo/camera_app.mojom-lite.js",
     IDR_CAMERA_CAMERA_APP_MOJOM_LITE_JS},
    {"js/mojo/mojo_bindings_lite.js", IDR_MOJO_MOJO_BINDINGS_LITE_JS},
    {"js/mojo/camera_app_helper.mojom-lite.js",
     IDR_CAMERA_CAMERA_APP_HELPER_MOJOM_LITE_JS},
    {"js/mojo/time.mojom-lite.js", IDR_CAMERA_TIME_MOJOM_LITE_JS},
    {"js/mojo/idle_manager.mojom-lite.js",
     IDR_CAMERA_IDLE_MANAGER_MOJOM_LITE_JS},
    {"js/mojo/camera_app.mojom-lite.js",
     IDR_CAMERA_CAMERA_APP_MOJOM_LITE_JS},
    {"js/mojo/geometry.mojom-lite.js", IDR_CAMERA_GEOMETRY_MOJOM_LITE_JS},
    {"js/mojo/range.mojom-lite.js", IDR_CAMERA_RANGE_MOJOM_LITE_JS},
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_RESOURCES_H_
