// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/linux/system_media_controls_linux.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/dbus/properties/dbus_properties.h"
#include "components/dbus/properties/success_barrier_callback.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/system_media_controls/system_media_controls_observer.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace system_media_controls {

// static
SystemMediaControls* SystemMediaControls::GetInstance() {
  internal::SystemMediaControlsLinux* service =
      internal::SystemMediaControlsLinux::GetInstance();
  service->StartService();
  return service;
}

namespace internal {

namespace {

constexpr int kNumMethodsToExport = 11;

}  // namespace

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kMprisAPIServiceNamePrefix[] =
    "org.mpris.MediaPlayer2.chrome.instance";
#else
const char kMprisAPIServiceNamePrefix[] =
    "org.mpris.MediaPlayer2.chromium.instance";
#endif
const char kMprisAPIObjectPath[] = "/org/mpris/MediaPlayer2";
const char kMprisAPIInterfaceName[] = "org.mpris.MediaPlayer2";
const char kMprisAPIPlayerInterfaceName[] = "org.mpris.MediaPlayer2.Player";

// static
SystemMediaControlsLinux* SystemMediaControlsLinux::GetInstance() {
  return base::Singleton<SystemMediaControlsLinux>::get();
}

SystemMediaControlsLinux::SystemMediaControlsLinux()
    : service_name_(std::string(kMprisAPIServiceNamePrefix) +
                    base::NumberToString(base::Process::Current().Pid())) {}

SystemMediaControlsLinux::~SystemMediaControlsLinux() {
  if (bus_) {
    dbus_thread_linux::GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
  }
}

void SystemMediaControlsLinux::StartService() {
  if (started_)
    return;
  started_ = true;
  InitializeDbusInterface();
}

void SystemMediaControlsLinux::AddObserver(
    SystemMediaControlsObserver* observer) {
  observers_.AddObserver(observer);

  // If the service is already ready, inform the observer.
  if (service_ready_)
    observer->OnServiceReady();
}

void SystemMediaControlsLinux::RemoveObserver(
    SystemMediaControlsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SystemMediaControlsLinux::SetIsNextEnabled(bool value) {
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "CanGoNext",
                           DbusBoolean(value));
}

void SystemMediaControlsLinux::SetIsPreviousEnabled(bool value) {
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "CanGoPrevious",
                           DbusBoolean(value));
}

void SystemMediaControlsLinux::SetIsPlayPauseEnabled(bool value) {
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "CanPlay",
                           DbusBoolean(value));
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "CanPause",
                           DbusBoolean(value));
}

void SystemMediaControlsLinux::SetPlaybackStatus(PlaybackStatus value) {
  auto status = [&]() {
    switch (value) {
      case PlaybackStatus::kPlaying:
        return DbusString("Playing");
      case PlaybackStatus::kPaused:
        return DbusString("Paused");
      case PlaybackStatus::kStopped:
        return DbusString("Stopped");
    }
  };
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "PlaybackStatus",
                           status());
}

void SystemMediaControlsLinux::SetTitle(const base::string16& value) {
  SetMetadataPropertyInternal(
      "xesam:title", MakeDbusVariant(DbusString(base::UTF16ToUTF8(value))));
}

void SystemMediaControlsLinux::SetArtist(const base::string16& value) {
  SetMetadataPropertyInternal(
      "xesam:artist",
      MakeDbusVariant(MakeDbusArray(DbusString(base::UTF16ToUTF8(value)))));
}

void SystemMediaControlsLinux::SetAlbum(const base::string16& value) {
  SetMetadataPropertyInternal(
      "xesam:album", MakeDbusVariant(DbusString(base::UTF16ToUTF8(value))));
}

void SystemMediaControlsLinux::ClearMetadata() {
  SetTitle(base::string16());
  SetArtist(base::string16());
  SetAlbum(base::string16());
}

std::string SystemMediaControlsLinux::GetServiceName() const {
  return service_name_;
}

void SystemMediaControlsLinux::InitializeProperties() {
  // org.mpris.MediaPlayer2 interface properties.
  auto set_property = [&](const std::string& property_name, auto&& value) {
    properties_->SetProperty(kMprisAPIInterfaceName, property_name,
                             std::forward<decltype(value)>(value), false);
  };
  set_property("CanQuit", DbusBoolean(false));
  set_property("CanRaise", DbusBoolean(false));
  set_property("HasTrackList", DbusBoolean(false));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  set_property("Identity", DbusString("Chrome"));
#else
  set_property("Identity", DbusString("Chromium"));
#endif
  set_property("SupportedUriSchemes", DbusArray<DbusString>());
  set_property("SupportedMimeTypes", DbusArray<DbusString>());

  // org.mpris.MediaPlayer2.Player interface properties.
  auto set_player_property = [&](const std::string& property_name,
                                 auto&& value) {
    properties_->SetProperty(kMprisAPIPlayerInterfaceName, property_name,
                             std::forward<decltype(value)>(value), false);
  };
  set_player_property("PlaybackStatus", DbusString("Stopped"));
  set_player_property("Rate", DbusDouble(1.0));
  set_player_property("Metadata", DbusDictionary());
  set_player_property("Volume", DbusDouble(1.0));
  set_player_property("Position", DbusInt64(0));
  set_player_property("MinimumRate", DbusDouble(1.0));
  set_player_property("MaximumRate", DbusDouble(1.0));
  set_player_property("CanGoNext", DbusBoolean(false));
  set_player_property("CanGoPrevious", DbusBoolean(false));
  set_player_property("CanPlay", DbusBoolean(false));
  set_player_property("CanPause", DbusBoolean(false));
  set_player_property("CanSeek", DbusBoolean(false));
  set_player_property("CanControl", DbusBoolean(true));
}

void SystemMediaControlsLinux::InitializeDbusInterface() {
  // Bus may be set for testing.
  if (!bus_) {
    dbus::Bus::Options bus_options;
    bus_options.bus_type = dbus::Bus::SESSION;
    bus_options.connection_type = dbus::Bus::PRIVATE;
    bus_options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
    bus_ = base::MakeRefCounted<dbus::Bus>(bus_options);
  }

  exported_object_ =
      bus_->GetExportedObject(dbus::ObjectPath(kMprisAPIObjectPath));
  int num_methods_attempted_to_export = 0;

  // kNumMethodsToExport calls for method export, 1 call for properties
  // initialization.
  barrier_ = SuccessBarrierCallback(
      kNumMethodsToExport + 1,
      base::BindOnce(&SystemMediaControlsLinux::OnInitialized,
                     base::Unretained(this)));

  properties_ = std::make_unique<DbusProperties>(exported_object_, barrier_);
  properties_->RegisterInterface(kMprisAPIInterfaceName);
  properties_->RegisterInterface(kMprisAPIPlayerInterfaceName);
  InitializeProperties();

  // Helper lambdas for exporting methods while keeping track of the number of
  // exported methods.
  auto export_method =
      [&](const std::string& interface_name, const std::string& method_name,
          dbus::ExportedObject::MethodCallCallback method_call_callback) {
        exported_object_->ExportMethod(
            interface_name, method_name, method_call_callback,
            base::BindRepeating(&SystemMediaControlsLinux::OnExported,
                                base::Unretained(this)));
        num_methods_attempted_to_export++;
      };
  auto export_unhandled_method = [&](const std::string& interface_name,
                                     const std::string& method_name) {
    export_method(interface_name, method_name,
                  base::BindRepeating(&SystemMediaControlsLinux::DoNothing,
                                      base::Unretained(this)));
  };

  // Set up org.mpris.MediaPlayer2 interface.
  // https://specifications.freedesktop.org/mpris-spec/2.2/Media_Player.html
  export_unhandled_method(kMprisAPIInterfaceName, "Raise");
  export_unhandled_method(kMprisAPIInterfaceName, "Quit");

  // Set up org.mpris.MediaPlayer2.Player interface.
  // https://specifications.freedesktop.org/mpris-spec/2.2/Player_Interface.html
  export_method(kMprisAPIPlayerInterfaceName, "Next",
                base::BindRepeating(&SystemMediaControlsLinux::Next,
                                    base::Unretained(this)));
  export_method(kMprisAPIPlayerInterfaceName, "Previous",
                base::BindRepeating(&SystemMediaControlsLinux::Previous,
                                    base::Unretained(this)));
  export_method(kMprisAPIPlayerInterfaceName, "Pause",
                base::BindRepeating(&SystemMediaControlsLinux::Pause,
                                    base::Unretained(this)));
  export_method(kMprisAPIPlayerInterfaceName, "PlayPause",
                base::BindRepeating(&SystemMediaControlsLinux::PlayPause,
                                    base::Unretained(this)));
  export_method(kMprisAPIPlayerInterfaceName, "Stop",
                base::BindRepeating(&SystemMediaControlsLinux::Stop,
                                    base::Unretained(this)));
  export_method(kMprisAPIPlayerInterfaceName, "Play",
                base::BindRepeating(&SystemMediaControlsLinux::Play,
                                    base::Unretained(this)));
  export_unhandled_method(kMprisAPIPlayerInterfaceName, "Seek");
  export_unhandled_method(kMprisAPIPlayerInterfaceName, "SetPosition");
  export_unhandled_method(kMprisAPIPlayerInterfaceName, "OpenUri");

  DCHECK_EQ(kNumMethodsToExport, num_methods_attempted_to_export);
}

void SystemMediaControlsLinux::OnExported(const std::string& interface_name,
                                          const std::string& method_name,
                                          bool success) {
  barrier_.Run(success);
}

void SystemMediaControlsLinux::OnInitialized(bool success) {
  if (success) {
    bus_->RequestOwnership(
        service_name_, dbus::Bus::ServiceOwnershipOptions::REQUIRE_PRIMARY,
        base::BindRepeating(&SystemMediaControlsLinux::OnOwnership,
                            base::Unretained(this)));
  }
}

void SystemMediaControlsLinux::OnOwnership(const std::string& service_name,
                                           bool success) {
  if (!success)
    return;

  service_ready_ = true;

  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnServiceReady();
}

void SystemMediaControlsLinux::Next(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnNext();
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::Previous(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPrevious();
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::Pause(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPause();
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::PlayPause(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPlayPause();
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::Stop(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnStop();
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::Play(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPlay();
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::DoNothing(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::SetMetadataPropertyInternal(
    const std::string& property_name,
    DbusVariant&& new_value) {
  DbusVariant* dictionary_variant =
      properties_->GetProperty(kMprisAPIPlayerInterfaceName, "Metadata");
  DCHECK(dictionary_variant);
  DbusDictionary* dictionary = dictionary_variant->GetAs<DbusDictionary>();
  DCHECK(dictionary);
  if (dictionary->Put(property_name, std::move(new_value)))
    properties_->PropertyUpdated(kMprisAPIPlayerInterfaceName, "Metadata");
}

}  // namespace internal

}  // namespace system_media_controls
