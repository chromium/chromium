// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_COORDINATOR_H_

#include <stddef.h>

#include <optional>
#include <string>

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_coordinator.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/browser/ui/views/media_preview/mic_preview/mic_coordinator.h"
#include "components/prefs/pref_service.h"

namespace views {
class View;
}  // namespace views

// MediaCoordinator sets up the media views.
class MediaCoordinator {
 public:
  enum class ViewType { kBoth, kCameraOnly, kMicOnly };

  // Specifies a selected device. Non-empty strings will cause the preview to
  // display only that device and disable the combobox.
  struct EligibleDevices {
    EligibleDevices();
    EligibleDevices(std::vector<std::string> cameras,
                    std::vector<std::string> mics);
    ~EligibleDevices();
    EligibleDevices(const EligibleDevices&);

    std::vector<std::string> cameras;
    std::vector<std::string> mics;
  };

  MediaCoordinator(ViewType view_type,
                   views::View& parent_view,
                   bool is_subsection,
                   EligibleDevices eligible_devices,
                   PrefService& prefs,
                   bool allow_device_selection,
                   const media_preview_metrics::Context& metrics_context);
  MediaCoordinator(const MediaCoordinator&) = delete;
  MediaCoordinator& operator=(const MediaCoordinator&) = delete;
  ~MediaCoordinator();

  void UpdateDevicePreferenceRanking();

  void OnCameraPermissionChange(bool has_permission);

 private:
  raw_ptr<MediaView> media_view_ = nullptr;
  std::optional<CameraCoordinator> camera_coordinator_;
  std::optional<MicCoordinator> mic_coordinator_;
};

namespace media_coordinator {

media_preview_metrics::PreviewType GetPreviewTypeFromMediaCoordinatorViewType(
    MediaCoordinator::ViewType view_type);

}  // namespace media_coordinator

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_COORDINATOR_H_
