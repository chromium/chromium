// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_ONE_ORIGIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_ONE_ORIGIN_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/media_preview/permission_prompt_previews_coordinator.h"
#include "components/media_effects/media_device_info.h"
#endif

// Bubble that prompts the user to grant or deny a permission request from one
// origin.
//
// ----------------------------------------------
// |                                       [ X ]|
// | Prompt title                               |
// | ------------------------------------------ |
// | 1+ rows with requests                      |
// | e.g. [LocationIcon] Know your location     |
// | ------------------------------------------ |
// | Extra text                                 |
// | ------------------------------------------ |
// |                        [ Block ] [ Allow ] |
// ----------------------------------------------
class PermissionPromptBubbleOneOriginView :
#if !BUILDFLAG(IS_CHROMEOS)
    public media_effects::MediaDeviceInfo::Observer,
#endif
    public PermissionPromptBubbleBaseView {
 public:
  PermissionPromptBubbleOneOriginView(
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
      base::TimeTicks permission_requested_time,
      PermissionPromptStyle prompt_style);
  PermissionPromptBubbleOneOriginView(
      const PermissionPromptBubbleOneOriginView&) = delete;
  PermissionPromptBubbleOneOriginView& operator=(
      const PermissionPromptBubbleOneOriginView&) = delete;
  ~PermissionPromptBubbleOneOriginView() override;

  // PermissionPromptBubbleBaseView:
  void RunButtonCallback(int button_id) override;

#if !BUILDFLAG(IS_CHROMEOS)
  const std::optional<PermissionPromptPreviewsCoordinator>&
  GetMediaPreviewsForTesting() const {
    return media_previews_;
  }
  const raw_ptr<views::Label> GetCameraPermissionLabelForTesting() const {
    return camera_permission_label_;
  }
  const raw_ptr<views::Label> GetPtzCameraPermissionLabelForTesting() const {
    return ptz_camera_permission_label_;
  }
  const raw_ptr<views::Label> GetMicPermissionLabelForTesting() const {
    return mic_permission_label_;
  }
#endif

 private:
  // Add a line for the |request| at |index| of the view.
  void AddRequestLine(permissions::PermissionRequest* request,
                      std::size_t index);

  // Adds Media (Camera / Mic) live preview feeds.
  void MaybeAddMediaPreview(
      std::vector<std::string> requested_audio_capture_device_id,
      std::vector<std::string> requested_video_capture_device_id,
      size_t index);

#if !BUILDFLAG(IS_CHROMEOS)
  // media_effects::MediaDeviceInfo::Observer overrides.
  void OnAudioDevicesChanged(
      const std::optional<std::vector<media::AudioDeviceDescription>>&
          device_infos) override;
  void OnVideoDevicesChanged(
      const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
          device_infos) override;
  std::optional<PermissionPromptPreviewsCoordinator> media_previews_;
  raw_ptr<views::Label> camera_permission_label_ = nullptr;
  raw_ptr<views::Label> ptz_camera_permission_label_ = nullptr;
  raw_ptr<views::Label> mic_permission_label_ = nullptr;
  base::ScopedObservation<media_effects::MediaDeviceInfo,
                          PermissionPromptBubbleOneOriginView>
      devices_observer_{this};
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_ONE_ORIGIN_VIEW_H_
