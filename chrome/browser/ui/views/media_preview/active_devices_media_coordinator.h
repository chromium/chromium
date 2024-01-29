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
#include "content/public/browser/web_contents.h"

class ActiveDevicesMediaCoordinator
    : public MediaCaptureDevicesDispatcher::Observer {
 public:
  ActiveDevicesMediaCoordinator(content::WebContents* web_contents,
                                MediaCoordinator::ViewType view_type,
                                views::View* parent_view);
  ActiveDevicesMediaCoordinator(const ActiveDevicesMediaCoordinator&) = delete;
  ActiveDevicesMediaCoordinator& operator=(
      const ActiveDevicesMediaCoordinator&) = delete;
  ~ActiveDevicesMediaCoordinator() override;

  void UpdateDevicePreferenceRanking();

 private:
  void UpdateMediaCoordinatorList();

  void GotDeviceIdsOpenedForWebContents(
      std::vector<std::string> active_device_ids);

  void AddMediaCoordinatorForDevice(
      const std::optional<std::string>& active_device_id);

  // MediaCaptureDevicesDispatcher::Observer impl.
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  std::vector<std::string> GetMediaCoordinatorKeys();

  base::WeakPtr<content::WebContents> web_contents_;
  MediaCoordinator::ViewType view_type_;
  raw_ptr<views::View> parent_view_;
  blink::mojom::MediaStreamType stream_type_;
  raw_ptr<views::View> container_;
  base::flat_map<std::string, std::unique_ptr<MediaCoordinator>>
      media_coordinators_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_ACTIVE_DEVICES_MEDIA_COORDINATOR_H_
