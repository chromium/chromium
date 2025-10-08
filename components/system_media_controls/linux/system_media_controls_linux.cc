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

SystemMediaControlsLinux::~SystemMediaControlsLinux() = default;

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
  properties_->SetProperty<"b">(kMprisAPIPlayerInterfaceName, "CanGoNext",
                                value);
}

void SystemMediaControlsLinux::SetIsPreviousEnabled(bool value) {
  properties_->SetProperty<"b">(kMprisAPIPlayerInterfaceName, "CanGoPrevious",
                                value);
}

void SystemMediaControlsLinux::SetIsPlayPauseEnabled(bool value) {
  properties_->SetProperty<"b">(kMprisAPIPlayerInterfaceName, "CanPlay", value);
  properties_->SetProperty<"b">(kMprisAPIPlayerInterfaceName, "CanPause",
                                value);
}

void SystemMediaControlsLinux::SetIsSeekToEnabled(bool value) {
  properties_->SetProperty<"b">(kMprisAPIPlayerInterfaceName, "CanSeek", value);
}

void SystemMediaControlsLinux::SetPlaybackStatus(PlaybackStatus value) {
  auto status = [&]() {
    switch (value) {
      case PlaybackStatus::kPlaying:
        return "Playing";
      case PlaybackStatus::kPaused:
        return "Paused";
      case PlaybackStatus::kStopped:
        return "Stopped";
    }
  };
  properties_->SetProperty<"s">(kMprisAPIPlayerInterfaceName, "PlaybackStatus",
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

  track_id_ =
      base::StringPrintf(kMprisAPICurrentTrackPathFormatString, value->c_str());
  UpdateMetadata();
}

void SystemMediaControlsLinux::SetTitle(const std::u16string& value) {
  title_ = base::UTF16ToUTF8(value);
  UpdateMetadata();
}

void SystemMediaControlsLinux::SetArtist(const std::u16string& value) {
  artist_ = {base::UTF16ToUTF8(value)};
  UpdateMetadata();
}

void SystemMediaControlsLinux::SetAlbum(const std::u16string& value) {
  album_ = base::UTF16ToUTF8(value);
  UpdateMetadata();
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
  track_id_ = std::nullopt;
  title_ = std::nullopt;
  artist_ = std::nullopt;
  album_ = std::nullopt;
  length_ = std::nullopt;
  art_url_ = std::nullopt;
  UpdateMetadata();
  thumbnail_.Reset();
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
  SetProperty<"b">("CanQuit", false);
  SetProperty<"b">("CanRaise", false);
  SetProperty<"b">("HasTrackList", false);

  SetProperty<"s">("Identity", product_name_);
  SetProperty<"as">("SupportedUriSchemes", {});
  SetProperty<"as">("SupportedMimeTypes", {});

  // org.mpris.MediaPlayer2.Player interface properties.
  SetPlayerProperty<"s">("PlaybackStatus", "Stopped");
  SetPlayerProperty<"d">("Rate", 1.0);
  SetPlayerProperty<"a{sv}">("Metadata", {});
  SetPlayerProperty<"d">("Volume", 1.0);
  SetPlayerProperty<"x">("Position", 0);
  SetPlayerProperty<"d">("MinimumRate", 1.0);
  SetPlayerProperty<"d">("MaximumRate", 1.0);
  SetPlayerProperty<"b">("CanGoNext", false);
  SetPlayerProperty<"b">("CanGoPrevious", false);
  SetPlayerProperty<"b">("CanPlay", false);
  SetPlayerProperty<"b">("CanPause", false);
  SetPlayerProperty<"b">("CanSeek", false);
  SetPlayerProperty<"b">("CanControl", true);
}

void SystemMediaControlsLinux::InitializeDbusInterface() {
  // Bus may be set for testing.
  if (!bus_) {
    bus_ = dbus_thread_linux::GetSharedSessionBus();
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

void SystemMediaControlsLinux::UpdateMetadata() {
  std::map<std::string, dbus_utils::Variant> metadata;

  if (track_id_.has_value()) {
    metadata["mpris:trackid"] =
        dbus_utils::Variant::Wrap<"o">(dbus::ObjectPath(*track_id_));
  }
  if (title_.has_value()) {
    metadata["xesam:title"] = dbus_utils::Variant::Wrap<"s">(*title_);
  }
  if (artist_.has_value()) {
    metadata["xesam:artist"] = dbus_utils::Variant::Wrap<"as">(*artist_);
  }
  if (album_.has_value()) {
    metadata["xesam:album"] = dbus_utils::Variant::Wrap<"s">(*album_);
  }
  if (length_.has_value()) {
    metadata["mpris:length"] = dbus_utils::Variant::Wrap<"x">(*length_);
  }
  if (art_url_.has_value()) {
    metadata["mpris:artUrl"] = dbus_utils::Variant::Wrap<"s">(*art_url_);
  }

  properties_->SetProperty<"a{sv}">(kMprisAPIPlayerInterfaceName, "Metadata",
                                    std::move(metadata));
}

void SystemMediaControlsLinux::DoNothing(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

void SystemMediaControlsLinux::ClearTrackId() {
  track_id_ = kMprisAPINoTrackPath;
  UpdateMetadata();
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
  properties_->SetProperty<"x">(kMprisAPIPlayerInterfaceName, "Position",
                                position, /*emit_signal=*/false);

  properties_->SetProperty<"d">(kMprisAPIPlayerInterfaceName, "Rate", rate,
                                emit_signal);
  length_ = duration;
  UpdateMetadata();

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
  art_url_ = path.empty() ? "" : "file://" + base::EscapePath(path.value());
  UpdateMetadata();
  thumbnail_ = std::move(thumbnail.second);
}

}  // namespace internal

}  // namespace system_media_controls
