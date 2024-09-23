// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_ACTIVE_DEVICES_MEDIA_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_ACTIVE_DEVICES_MEDIA_COORDINATOR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/views/media_preview/media_coordinator.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/separator.h"

class ActiveDevicesMediaCoordinator
    : public MediaCaptureDevicesDispatcher::Observer {
 public:
  ActiveDevicesMediaCoordinator(
      base::WeakPtr<content::WebContents> web_contents,
      MediaCoordinator::ViewType view_type,
      MediaView* container,
      media_preview_metrics::Context metrics_context);
  ActiveDevicesMediaCoordinator(const ActiveDevicesMediaCoordinator&) = delete;
  ActiveDevicesMediaCoordinator& operator=(
      const ActiveDevicesMediaCoordinator&) = delete;
  ~ActiveDevicesMediaCoordinator() override;

  void UpdateDevicePreferenceRanking();

  std::vector<std::string> GetMediaCoordinatorKeys();

  // MediaCaptureDevicesDispatcher::Observer impl.
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  void OnPermissionChange(bool has_permission);

 private:
  void UpdateMediaCoordinatorList();

  void GotDeviceIdsOpenedForWebContents(
      std::vector<std::string> active_device_ids);

  void CreateMutableCoordinator();

  void CreateImmutableCoordinators(std::vector<std::string> active_device_ids);

  void AddMediaCoordinatorForDevice(
      const std::optional<std::string>& active_device_id);

  const base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<MediaView> container_;
  const MediaCoordinator::ViewType view_type_;
  const blink::mojom::MediaStreamType stream_type_;
  const media_preview_metrics::Context media_preview_metrics_context_;
  bool permission_allowed_ = false;
  base::flat_map<std::string, std::unique_ptr<MediaCoordinator>>
      media_coordinators_;
  base::flat_map<std::string, raw_ptr<views::Separator>> separators_;
  base::ScopedObservation<MediaCaptureDevicesDispatcher,
                          ActiveDevicesMediaCoordinator>
      media_devices_dispatcher_observer_{this};

  base::WeakPtrFactory<ActiveDevicesMediaCoordinator> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_ACTIVE_DEVICES_MEDIA_COORDINATOR_H_
