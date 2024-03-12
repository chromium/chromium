// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/linux/system_media_controls_linux.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notimplemented.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/dbus/properties/dbus_properties.h"
#include "components/dbus/properties/success_barrier_callback.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/system_media_controls/system_media_controls_observer.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace system_media_controls {

// static
std::unique_ptr<SystemMediaControls> SystemMediaControls::Create(
    const std::string& product_name,
    int window) {
  auto service =
      std::make_unique<internal::SystemMediaControlsLinux>(product_name);
  service->StartService();
  return std::move(service);
}

// static
void SystemMediaControls::SetVisibilityChangedCallbackForTesting(
    base::RepeatingCallback<void(bool)>*) {
  NOTIMPLEMENTED();
}

namespace internal {

namespace {

constexpr int kNumMethodsToExport = 11;

constexpr base::TimeDelta kUpdatePositionInterval = base::Milliseconds(100);

const char kMprisAPINoTrackPath[] = "/org/mpris/MediaPlayer2/TrackList/NoTrack";

const char kMprisAPICurrentTrackPathFormatString[] =
    "/org/chromium/MediaPlayer2/TrackList/Track%s";

// Writes `bitmap` to a new temporary PNG file and returns a a pair of the file
// path and a managed base::ScopedTempFile bound to this sequence.  This should
// be called on the file task runner so the file is cleaned up on the proper
// sequence.  The path will be empty if the image was empty or the image failed
// to write to the file.
std::pair<base::FilePath, base::SequenceBound<base::ScopedTempFile>>
WriteBitmapToTmpFile(const SkBitmap& bitmap) {
  if (bitmap.empty()) {
    return {};
  }

  gfx::Image image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  auto data = image.As1xPNGBytes();

  if (data->size() == 0) {
    return {};
  }

  base::ScopedTempFile scoped_file;
  if (!scoped_file.Create()) {
    return {};
  }

  if (!base::WriteFile(scoped_file.path(), *data)) {
    return {};
  }

  // Make a copy of the path before `scoped_file` is moved.
  base::FilePath path = scoped_file.path();
  return std::make_pair(std::move(path),
                        base::SequenceBound<base::ScopedTempFile>(
                            base::SequencedTaskRunner::GetCurrentDefault(),
                            std::move(scoped_file)));
}

}  // namespace

const char kMprisAPIServiceNameFormatString[] =
    "org.mpris.MediaPlayer2.chromium.instance%i";
const char kMprisAPIObjectPath[] = "/org/mpris/MediaPlayer2";
const char kMprisAPIInterfaceName[] = "org.mpris.MediaPlayer2";
const char kMprisAPIPlayerInterfaceName[] = "org.mpris.MediaPlayer2.Player";
const char kMprisAPISignalSeeked[] = "Seeked";

SystemMediaControlsLinux::SystemMediaControlsLinux(
    const std::string& product_name)
    : product_name_(product_name),
      service_name_(base::StringPrintf(kMprisAPIServiceNameFormatString,
                                       base::Process::Current().Pid())),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

SystemMediaControlsLinux::~SystemMediaControlsLinux() {
  if (bus_) {
    dbus_thread_linux::GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, bus_));
  }
}

void SystemMediaControlsLinux::StartService() {
  if (started_) {
    return;
  }
  started_ = true;
  InitializeDbusInterface();
}

void SystemMediaControlsLinux::AddObserver(
    SystemMediaControlsObserver* observer) {
  observers_.AddObserver(observer);

  // If the service is already ready, inform the observer.
  if (service_ready_) {
    observer->OnServiceReady();
  }
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

void SystemMediaControlsLinux::SetIsSeekToEnabled(bool value) {
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "CanSeek",
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

  playing_ = (value == PlaybackStatus::kPlaying);
  if (playing_ && position_.has_value()) {
    StartPositionUpdateTimer();
  } else {
    StopPositionUpdateTimer();
  }
}

void SystemMediaControlsLinux::SetID(const std::string* value) {
  if (!value) {
    ClearTrackId();
    return;
  }

  const std::string track_id =
      base::StringPrintf(kMprisAPICurrentTrackPathFormatString, value->c_str());
  SetMetadataPropertyInternal(
      "mpris:trackid",
      MakeDbusVariant(DbusObjectPath(dbus::ObjectPath(track_id))));
}

void SystemMediaControlsLinux::SetTitle(const std::u16string& value) {
  SetMetadataPropertyInternal(
      "xesam:title", MakeDbusVariant(DbusString(base::UTF16ToUTF8(value))));
}

void SystemMediaControlsLinux::SetArtist(const std::u16string& value) {
  SetMetadataPropertyInternal(
      "xesam:artist",
      MakeDbusVariant(MakeDbusArray(DbusString(base::UTF16ToUTF8(value)))));
}

void SystemMediaControlsLinux::SetAlbum(const std::u16string& value) {
  SetMetadataPropertyInternal(
      "xesam:album", MakeDbusVariant(DbusString(base::UTF16ToUTF8(value))));
}

void SystemMediaControlsLinux::SetThumbnail(const SkBitmap& bitmap) {
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&WriteBitmapToTmpFile, bitmap),
      base::BindOnce(&SystemMediaControlsLinux::OnThumbnailFileWritten,
                     weak_factory_.GetWeakPtr()));
}

void SystemMediaControlsLinux::SetPosition(
    const media_session::MediaPosition& position) {
  position_ = position;
  UpdatePosition(/*emit_signal=*/true);

  if (playing_) {
    StartPositionUpdateTimer();
  }
}

void SystemMediaControlsLinux::ClearMetadata() {
  SetTitle(std::u16string());
  SetArtist(std::u16string());
  SetAlbum(std::u16string());
  SetThumbnail(SkBitmap());
  ClearTrackId();
  ClearPosition();
}

bool SystemMediaControlsLinux::GetVisibilityForTesting() const {
  NOTIMPLEMENTED();
  return false;
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

  set_property("Identity", DbusString(product_name_));
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
  export_method(kMprisAPIPlayerInterfaceName, "Seek",
                base::BindRepeating(&SystemMediaControlsLinux::Seek,
                                    base::Unretained(this)));
  export_method(kMprisAPIPlayerInterfaceName, "SetPosition",
                base::BindRepeating(&SystemMediaControlsLinux::SetPositionMpris,
                                    base::Unretained(this)));
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
  if (!success) {
    return;
  }

  service_ready_ = true;

  for (SystemMediaControlsObserver& obs : observers_) {
    obs.OnServiceReady();
  }
}

void SystemMediaControlsLinux::Next(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_) {
    obs.OnNext(this);
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::Previous(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_) {
    obs.OnPrevious(this);
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::Pause(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_) {
    obs.OnPause(this);
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::PlayPause(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_) {
    obs.OnPlayPause(this);
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::Stop(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_) {
    obs.OnStop(this);
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::Play(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  for (SystemMediaControlsObserver& obs : observers_) {
    obs.OnPlay(this);
  }
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::Seek(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  int64_t offset;
  dbus::MessageReader reader(method_call);
  if (!reader.PopInt64(&offset)) {
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
    return;
  }

  for (SystemMediaControlsObserver& obs : observers_) {
    obs.OnSeek(this, base::Microseconds(offset));
  }

  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::SetPositionMpris(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::ObjectPath track_id;
  int64_t position;
  dbus::MessageReader reader(method_call);

  if (!reader.PopObjectPath(&track_id)) {
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
    return;
  }

  if (!reader.PopInt64(&position)) {
    std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
    return;
  }

  for (SystemMediaControlsObserver& obs : observers_) {
    obs.OnSeekTo(this, base::Microseconds(position));
  }

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
  if (dictionary->Put(property_name, std::move(new_value))) {
    properties_->PropertyUpdated(kMprisAPIPlayerInterfaceName, "Metadata");
  }
}

void SystemMediaControlsLinux::ClearTrackId() {
  SetMetadataPropertyInternal(
      "mpris:trackid",
      MakeDbusVariant(DbusObjectPath(dbus::ObjectPath(kMprisAPINoTrackPath))));
}

void SystemMediaControlsLinux::ClearPosition() {
  position_ = std::nullopt;
  StopPositionUpdateTimer();
  UpdatePosition(/*emit_signal=*/true);
}

void SystemMediaControlsLinux::UpdatePosition(bool emit_signal) {
  int64_t position = 0;
  double rate = 1.0;
  int64_t duration = 0;

  if (position_.has_value()) {
    position = position_->GetPosition().InMicroseconds();
    rate = position_->playback_rate();
    duration = position_->duration().InMicroseconds();
  }

  // We never emit a PropertiesChanged signal for the "Position" property. We
  // only emit "Seeked" signals.
  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "Position",
                           DbusInt64(position), /*emit_signal=*/false);

  properties_->SetProperty(kMprisAPIPlayerInterfaceName, "Rate",
                           DbusDouble(rate), emit_signal);
  SetMetadataPropertyInternal("mpris:length",
                              MakeDbusVariant(DbusInt64(duration)));

  if (!service_ready_ || !emit_signal || !position_.has_value()) {
    return;
  }

  dbus::Signal seeked_signal(kMprisAPIPlayerInterfaceName,
                             kMprisAPISignalSeeked);
  dbus::MessageWriter writer(&seeked_signal);
  writer.AppendInt64(position);
  exported_object_->SendSignal(&seeked_signal);
}

void SystemMediaControlsLinux::StartPositionUpdateTimer() {
  // The timer should only run when the media is playing and has a position.
  DCHECK(playing_);
  DCHECK(position_.has_value());

  // base::Unretained(this) is safe here since |this| owns
  // |position_update_timer_|.
  position_update_timer_.Start(
      FROM_HERE, kUpdatePositionInterval,
      base::BindRepeating(&SystemMediaControlsLinux::UpdatePosition,
                          base::Unretained(this), /*emit_signal=*/false));
}

void SystemMediaControlsLinux::StopPositionUpdateTimer() {
  position_update_timer_.Stop();
}

void SystemMediaControlsLinux::OnThumbnailFileWritten(
    std::pair<base::FilePath, base::SequenceBound<base::ScopedTempFile>>
        thumbnail) {
  const auto& path = thumbnail.first;
  auto url = path.empty() ? "" : "file://" + base::EscapePath(path.value());
  SetMetadataPropertyInternal("mpris:artUrl", MakeDbusVariant(DbusString(url)));
  thumbnail_ = std::move(thumbnail.second);
}

}  // namespace internal

}  // namespace system_media_controls
