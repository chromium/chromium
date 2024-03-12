// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_MEDIA_CONTROLS_LINUX_SYSTEM_MEDIA_CONTROLS_LINUX_H_
#define COMPONENTS_SYSTEM_MEDIA_CONTROLS_LINUX_SYSTEM_MEDIA_CONTROLS_LINUX_H_

#include <optional>
#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "components/dbus/properties/types.h"
#include "components/system_media_controls/system_media_controls.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"

class DbusProperties;

namespace base {
class SequencedTaskRunner;
}

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace system_media_controls {

class SystemMediaControlsObserver;

namespace internal {

COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS)
extern const char kMprisAPIServiceNameFormatString[];
COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS) extern const char kMprisAPIObjectPath[];
COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS)
extern const char kMprisAPIInterfaceName[];
COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS)
extern const char kMprisAPIPlayerInterfaceName[];
COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS)
extern const char kMprisAPISignalSeeked[];

// A D-Bus service conforming to the MPRIS spec:
// https://specifications.freedesktop.org/mpris-spec/latest/
class COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS) SystemMediaControlsLinux
    : public SystemMediaControls {
 public:
  explicit SystemMediaControlsLinux(const std::string& product_name);

  SystemMediaControlsLinux(const SystemMediaControlsLinux&) = delete;
  SystemMediaControlsLinux& operator=(const SystemMediaControlsLinux&) = delete;

  ~SystemMediaControlsLinux() override;

  // Starts the DBus service.
  void StartService();

  // SystemMediaControls implementation.
  void AddObserver(SystemMediaControlsObserver* observer) override;
  void RemoveObserver(SystemMediaControlsObserver* observer) override;
  void SetEnabled(bool enabled) override {}
  void SetIsNextEnabled(bool value) override;
  void SetIsPreviousEnabled(bool value) override;
  void SetIsPlayPauseEnabled(bool value) override;
  void SetIsStopEnabled(bool value) override {}
  void SetIsSeekToEnabled(bool value) override;
  void SetPlaybackStatus(PlaybackStatus value) override;
  void SetID(const std::string* value) override;
  void SetTitle(const std::u16string& value) override;
  void SetArtist(const std::u16string& value) override;
  void SetAlbum(const std::u16string& value) override;
  void SetThumbnail(const SkBitmap& bitmap) override;
  void SetPosition(const media_session::MediaPosition& position) override;
  void ClearThumbnail() override {}
  void ClearMetadata() override;
  void UpdateDisplay() override {}
  bool GetVisibilityForTesting() const override;

  // Returns the generated service name.
  std::string GetServiceName() const;

  // Used for testing with a mock DBus Bus.
  void SetBusForTesting(scoped_refptr<dbus::Bus> bus) { bus_ = bus; }

 private:
  void InitializeProperties();
  void InitializeDbusInterface();
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);
  void OnInitialized(bool success);
  void OnOwnership(const std::string& service_name, bool success);

  // org.mpris.MediaPlayer2.Player interface.
  void Next(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);
  void Previous(dbus::MethodCall* method_call,
                dbus::ExportedObject::ResponseSender response_sender);
  void Pause(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender);
  void PlayPause(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);
  void Stop(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);
  void Play(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);
  void Seek(dbus::MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);
  void SetPositionMpris(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);

  // Used for API methods we don't support.
  void DoNothing(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);

  // Sets a value on the Metadata property map and sends a PropertiesChanged
  // signal if necessary.
  void SetMetadataPropertyInternal(const std::string& property_name,
                                   DbusVariant&& new_value);

  void ClearTrackId();

  void ClearPosition();

  // Updates MPRIS with our current position.
  void UpdatePosition(bool emit_signal);

  void StartPositionUpdateTimer();
  void StopPositionUpdateTimer();

  void OnThumbnailFileWritten(
      std::pair<base::FilePath, base::SequenceBound<base::ScopedTempFile>>
          thumbnail);

  std::optional<media_session::MediaPosition> position_;
  base::RepeatingTimer position_update_timer_;
  bool playing_ = false;

  const std::string product_name_;

  std::unique_ptr<DbusProperties> properties_;

  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ExportedObject, DanglingUntriaged> exported_object_;

  // The generated service name given to |bus_| when requesting ownership.
  const std::string service_name_;

  base::RepeatingCallback<void(bool)> barrier_;

  // True if we have started creating the DBus service.
  bool started_ = false;

  // True if we have finished creating the DBus service and received ownership.
  bool service_ready_ = false;

  // A temporary file containing the thumbnail image.
  base::SequenceBound<base::ScopedTempFile> thumbnail_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::ObserverList<SystemMediaControlsObserver> observers_;

  base::WeakPtrFactory<SystemMediaControlsLinux> weak_factory_{this};
};

}  // namespace internal

}  // namespace system_media_controls

#endif  // COMPONENTS_SYSTEM_MEDIA_CONTROLS_LINUX_SYSTEM_MEDIA_CONTROLS_LINUX_H_
