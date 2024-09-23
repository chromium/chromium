// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_LIST_HOST_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_LIST_HOST_H_

#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// Serves as an adapter between Media Router and Global Media Controls UI Mojo
// interfaces:
// - Receives Cast device updates via the CastDialogController::Observer
//   interface and forwards them to mojom::DeviceListClient.
// - Receives device picker UI events via the mojom::DeviceListHost interface
//   and forwards them to CastDialogController.
class CastDeviceListHost : public global_media_controls::mojom::DeviceListHost,
                           media_router::CastDialogController::Observer {
 public:
  CastDeviceListHost(
      std::unique_ptr<media_router::CastDialogController> dialog_controller,
      mojo::PendingRemote<global_media_controls::mojom::DeviceListClient>
          observer,
      base::RepeatingClosure media_remoting_callback,
      base::RepeatingClosure hide_dialog_callback,
      base::RepeatingClosure on_sinks_discovered_callback);
  ~CastDeviceListHost() override;

  // mojom::DeviceListHost:
  void SelectDevice(const std::string& device_id) override;

  // media_router::CastDialogController::Observer:
  void OnModelUpdated(const media_router::CastDialogModel& model) override;
  void OnCastingStarted() override;

  int id() const { return id_; }

 private:
  void StartCasting(const media_router::UIMediaSink& sink);
  void DestroyCastController();
  void RecordSinkLoadTime();

  // Used to generate `id_`.
  static int next_id_;

  std::unique_ptr<media_router::CastDialogController> cast_controller_;
  mojo::Remote<global_media_controls::mojom::DeviceListClient> client_;
  std::vector<media_router::UIMediaSink> sinks_;
  // Called whenever a Media Remoting session is starting.
  base::RepeatingClosure media_remoting_callback_;
  // Called whenever a tab mirroring session starts.
  base::RepeatingClosure hide_dialog_callback_;
  // Called whenever the sink is discovered.
  base::RepeatingClosure on_sinks_discovered_callback_;

  // Metrics
  base::Time initialization_time_;
  base::Time sinks_load_time_;

  const int id_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_LIST_HOST_H_
