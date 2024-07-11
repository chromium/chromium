// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/win/system_media_controls_win.h"

#include <systemmediatransportcontrolsinterop.h>
#include <windows.media.control.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "components/system_media_controls/system_media_controls_observer.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace system_media_controls {

// For testing only.
base::RepeatingCallback<void(bool)>*
    g_on_visibility_changed_for_testing_callback = nullptr;

// static
std::unique_ptr<SystemMediaControls> SystemMediaControls::Create(
    const std::string& product_name,
    int window) {
  auto service = std::make_unique<internal::SystemMediaControlsWin>(window);
  if (service->Initialize())
    return std::move(service);
  return nullptr;
}

// static
void SystemMediaControls::SetVisibilityChangedCallbackForTesting(
    base::RepeatingCallback<void(bool)>* callback) {
  CHECK_IS_TEST();
  g_on_visibility_changed_for_testing_callback = callback;
}

namespace internal {

using ABI::Windows::Media::IPlaybackPositionChangeRequestedEventArgs;
using ABI::Windows::Media::ISystemMediaTransportControls;
using ABI::Windows::Media::ISystemMediaTransportControls2;
using ABI::Windows::Media::ISystemMediaTransportControlsButtonPressedEventArgs;
using ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties;
using ABI::Windows::Media::PlaybackPositionChangeRequestedEventArgs;
using ABI::Windows::Media::SystemMediaTransportControls;
using ABI::Windows::Media::SystemMediaTransportControlsButton;
using ABI::Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs;
using ABI::Windows::Storage::Streams::IDataWriter;
using ABI::Windows::Storage::Streams::IDataWriterFactory;
using ABI::Windows::Storage::Streams::IOutputStream;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using ABI::Windows::Storage::Streams::IRandomAccessStreamReference;
using ABI::Windows::Storage::Streams::IRandomAccessStreamReferenceStatics;

SystemMediaControlsWin::SystemMediaControlsWin(int window)
    : is_for_web_app_(window != -1),
      web_app_window_(reinterpret_cast<HWND>(window)) {}

SystemMediaControlsWin::~SystemMediaControlsWin() {
  if (has_valid_button_pressed_registration_token_) {
    DCHECK(system_media_controls_);
    system_media_controls_->remove_ButtonPressed(
        button_pressed_registration_token_);
    if (has_valid_playback_position_change_requested_registration_token_) {
      Microsoft::WRL::ComPtr<ISystemMediaTransportControls2>
          system_media_controls_2;
      HRESULT hr = system_media_controls_.As(&system_media_controls_2);
      if (SUCCEEDED(hr)) {
        system_media_controls_2->remove_PlaybackPositionChangeRequested(
            playback_position_change_requested_registration_token_);
      }
    }
    ClearMetadata();
  }
}

bool SystemMediaControlsWin::Initialize() {
  if (attempted_to_initialize_)
    return initialized_;

  attempted_to_initialize_ = true;

  Microsoft::WRL::ComPtr<ISystemMediaTransportControlsInterop> interop;
  HRESULT hr = base::win::GetActivationFactory<
      ISystemMediaTransportControlsInterop,
      RuntimeClass_Windows_Media_SystemMediaTransportControls>(&interop);
  if (FAILED(hr))
    return false;

  if (is_for_web_app_) {
    hr = interop->GetForWindow(web_app_window_,
                               IID_PPV_ARGS(&system_media_controls_));
  } else {
    hr = interop->GetForWindow(gfx::SingletonHwnd::GetInstance()->hwnd(),
                               IID_PPV_ARGS(&system_media_controls_));
  }
  if (FAILED(hr))
    return false;

  auto weak_ptr = weak_factory_.GetWeakPtr();
  auto button_pressed_handler =
      Microsoft::WRL::Callback<ABI::Windows::Foundation::ITypedEventHandler<
          SystemMediaTransportControls*,
          SystemMediaTransportControlsButtonPressedEventArgs*>>(
          [weak_ptr](
              ISystemMediaTransportControls* sender,
              ISystemMediaTransportControlsButtonPressedEventArgs* args) {
            if (weak_ptr) {
              weak_ptr.get()->ButtonPressed(sender, args);
            }
            return S_OK;
          });
  hr = system_media_controls_->add_ButtonPressed(
      button_pressed_handler.Get(), &button_pressed_registration_token_);
  if (FAILED(hr))
    return false;

  has_valid_button_pressed_registration_token_ = true;

  hr = system_media_controls_->put_IsEnabled(true);
  if (FAILED(hr))
    return false;

  OnEnabledStatusChangedForTesting();

  hr = system_media_controls_->get_DisplayUpdater(&display_updater_);
  if (FAILED(hr))
    return false;

  // The current MediaSession API implementation matches the SMTC music type
  // most closely, since MediaSession has the artist property which the SMTC
  // only presents to music playback types.
  hr = display_updater_->put_Type(
      ABI::Windows::Media::MediaPlaybackType::MediaPlaybackType_Music);
  if (FAILED(hr))
    return false;

  hr = display_updater_->get_MusicProperties(&display_properties_);
  if (FAILED(hr))
    return false;

  initialized_ = true;
  return true;
}

void SystemMediaControlsWin::AddObserver(
    SystemMediaControlsObserver* observer) {
  observers_.AddObserver(observer);

  if (initialized_)
    observer->OnServiceReady();
}

void SystemMediaControlsWin::RemoveObserver(
    SystemMediaControlsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SystemMediaControlsWin::SetEnabled(bool enabled) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_IsEnabled(enabled);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetIsNextEnabled(bool value) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_IsNextEnabled(value);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetIsPreviousEnabled(bool value) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_IsPreviousEnabled(value);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetIsPlayPauseEnabled(bool value) {
  DCHECK(initialized_);

  HRESULT hr = system_media_controls_->put_IsPlayEnabled(value);
  DCHECK(SUCCEEDED(hr));

  hr = system_media_controls_->put_IsPauseEnabled(value);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetIsStopEnabled(bool value) {
  DCHECK(initialized_);
  HRESULT hr = system_media_controls_->put_IsStopEnabled(value);
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetIsSeekToEnabled(bool value) {
  DCHECK(initialized_);

  Microsoft::WRL::ComPtr<ISystemMediaTransportControls2>
      system_media_controls_2;
  HRESULT hr = system_media_controls_.As(&system_media_controls_2);
  if (FAILED(hr))
    return;

  if (value) {
    auto weak_ptr = weak_factory_.GetWeakPtr();
    auto playback_position_change_requested_handler =
        Microsoft::WRL::Callback<ABI::Windows::Foundation::ITypedEventHandler<
            SystemMediaTransportControls*,
            PlaybackPositionChangeRequestedEventArgs*>>(
            [weak_ptr](ISystemMediaTransportControls* sender,
                       IPlaybackPositionChangeRequestedEventArgs* args) {
              if (weak_ptr) {
                weak_ptr.get()->PlaybackPositionChangeRequested(sender, args);
              }
              return S_OK;
            });
    hr = system_media_controls_2->add_PlaybackPositionChangeRequested(
        playback_position_change_requested_handler.Get(),
        &playback_position_change_requested_registration_token_);
    DCHECK(SUCCEEDED(hr));
    has_valid_playback_position_change_requested_registration_token_ = true;
  } else {
    if (has_valid_playback_position_change_requested_registration_token_) {
      hr = system_media_controls_2->remove_PlaybackPositionChangeRequested(
          playback_position_change_requested_registration_token_);
      DCHECK(SUCCEEDED(hr));
      has_valid_playback_position_change_requested_registration_token_ = false;
    }
  }
}

void SystemMediaControlsWin::SetPlaybackStatus(PlaybackStatus status) {
  DCHECK(initialized_);
  HRESULT hr =
      system_media_controls_->put_PlaybackStatus(GetSmtcPlaybackStatus(status));
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetTitle(const std::u16string& title) {
  DCHECK(initialized_);
  DCHECK(display_properties_);
  base::win::ScopedHString h_title =
      base::win::ScopedHString::Create(base::UTF16ToWide(title));
  HRESULT hr = display_properties_->put_Title(h_title.get());
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetArtist(const std::u16string& artist) {
  DCHECK(initialized_);
  DCHECK(display_properties_);
  base::win::ScopedHString h_artist =
      base::win::ScopedHString::Create(base::UTF16ToWide(artist));
  HRESULT hr = display_properties_->put_Artist(h_artist.get());
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetThumbnail(const SkBitmap& bitmap) {
  DCHECK(initialized_);
  DCHECK(display_updater_);
  // Use |icon_data_writer_| to write the bitmap data into |icon_stream_| so we
  // can populate |icon_stream_reference_| and then give it to the SMTC. All of
  // these are member variables to avoid a race condition between them being
  // destructed and the async operation completing.
  base::win::ScopedHString id = base::win::ScopedHString::Create(
      RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream);
  HRESULT hr = base::win::RoActivateInstance(id.get(), &icon_stream_);
  DCHECK(SUCCEEDED(hr));

  Microsoft::WRL::ComPtr<IDataWriterFactory> data_writer_factory;
  hr = base::win::GetActivationFactory<
      IDataWriterFactory, RuntimeClass_Windows_Storage_Streams_DataWriter>(
      &data_writer_factory);
  DCHECK(SUCCEEDED(hr));

  Microsoft::WRL::ComPtr<IOutputStream> output_stream;
  hr = icon_stream_.As(&output_stream);
  DCHECK(SUCCEEDED(hr));

  hr = data_writer_factory->CreateDataWriter(output_stream.Get(),
                                             &icon_data_writer_);
  DCHECK(SUCCEEDED(hr));

  std::vector<unsigned char> icon_png;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &icon_png);
  hr = icon_data_writer_->WriteBytes(icon_png.size(), (BYTE*)icon_png.data());
  DCHECK(SUCCEEDED(hr));

  // Store the written bytes in the stream, an async operation.
  Microsoft::WRL::ComPtr<
      ABI::Windows::Foundation::IAsyncOperation<unsigned int>>
      store_async_operation;
  hr = icon_data_writer_->StoreAsync(&store_async_operation);
  DCHECK(SUCCEEDED(hr));

  // Make a callback that gives the icon to the SMTC once the bits make it into
  // |icon_stream_|
  auto store_async_callback = Microsoft::WRL::Callback<
      ABI::Windows::Foundation::IAsyncOperationCompletedHandler<unsigned int>>(
      [this](ABI::Windows::Foundation::IAsyncOperation<unsigned int>* async_op,
             ABI::Windows::Foundation::AsyncStatus status) mutable {
        // Check the async operation completed successfully.
        ABI::Windows::Foundation::IAsyncInfo* async_info;
        HRESULT hr = async_op->QueryInterface(
            IID_IAsyncInfo, reinterpret_cast<void**>(&async_info));
        DCHECK(SUCCEEDED(hr));
        async_info->get_ErrorCode(&hr);
        if (SUCCEEDED(hr) &&
            status == ABI::Windows::Foundation::AsyncStatus::Completed) {
          Microsoft::WRL::ComPtr<IRandomAccessStreamReferenceStatics>
              reference_statics;
          HRESULT result = base::win::GetActivationFactory<
              IRandomAccessStreamReferenceStatics,
              RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference>(
              &reference_statics);
          DCHECK(SUCCEEDED(result));

          result = reference_statics->CreateFromStream(icon_stream_.Get(),
                                                       &icon_stream_reference_);
          DCHECK(SUCCEEDED(result));

          result =
              display_updater_->put_Thumbnail(icon_stream_reference_.Get());
          DCHECK(SUCCEEDED(result));

          result = display_updater_->Update();
          DCHECK(SUCCEEDED(result));
        }
        return hr;
      });

  hr = store_async_operation->put_Completed(store_async_callback.Get());
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetPosition(
    const media_session::MediaPosition& position) {
  DCHECK(initialized_);

  Microsoft::WRL::ComPtr<ISystemMediaTransportControls2>
      system_media_controls_2;
  HRESULT hr = system_media_controls_.As(&system_media_controls_2);
  if (FAILED(hr))
    return;

  Microsoft::WRL::ComPtr<ISystemMediaTransportControlsTimelineProperties>
      timeline_properties;
  base::win::ScopedHString id = base::win::ScopedHString::Create(
      RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties);
  hr = base::win::RoActivateInstance(id.get(), &timeline_properties);
  DCHECK(SUCCEEDED(hr));

  ABI::Windows::Foundation::TimeSpan timeSpanZero = {0};

  hr = timeline_properties->put_MinSeekTime(timeSpanZero);
  DCHECK(SUCCEEDED(hr));

  hr = timeline_properties->put_StartTime(timeSpanZero);
  DCHECK(SUCCEEDED(hr));

  hr = timeline_properties->put_Position(
      position.GetPosition().ToWinrtTimeSpan());
  DCHECK(SUCCEEDED(hr));

  ABI::Windows::Foundation::TimeSpan duration =
      position.duration().ToWinrtTimeSpan();

  hr = timeline_properties->put_EndTime(duration);
  DCHECK(SUCCEEDED(hr));

  hr = timeline_properties->put_MaxSeekTime(duration);
  DCHECK(SUCCEEDED(hr));

  hr = system_media_controls_2->UpdateTimelineProperties(
      timeline_properties.Get());
  DCHECK(SUCCEEDED(hr));

  hr = system_media_controls_2->put_PlaybackRate(position.playback_rate());
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::ClearThumbnail() {
  DCHECK(initialized_);
  DCHECK(display_updater_);
  HRESULT hr = display_updater_->put_Thumbnail(nullptr);
  DCHECK(SUCCEEDED(hr));

  hr = display_updater_->Update();
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::ClearMetadata() {
  DCHECK(initialized_);
  DCHECK(display_updater_);
  HRESULT hr = display_updater_->ClearAll();
  DCHECK(SUCCEEDED(hr));

  // To prevent disabled controls and the executable name from showing up in the
  // SMTC, we need to tell them that we are disabled.
  hr = system_media_controls_->put_IsEnabled(false);
  DCHECK(SUCCEEDED(hr));

  OnEnabledStatusChangedForTesting();
}

void SystemMediaControlsWin::UpdateDisplay() {
  DCHECK(initialized_);
  DCHECK(system_media_controls_);
  DCHECK(display_updater_);
  HRESULT hr = system_media_controls_->put_IsEnabled(true);
  DCHECK(SUCCEEDED(hr));

  OnEnabledStatusChangedForTesting();

  // |ClearAll()| unsets the type, if we don't set it again then the artist
  // won't be displayed.
  hr = display_updater_->put_Type(
      ABI::Windows::Media::MediaPlaybackType::MediaPlaybackType_Music);
  DCHECK(SUCCEEDED(hr));

  hr = display_updater_->Update();
  DCHECK(SUCCEEDED(hr));
}

bool SystemMediaControlsWin::GetVisibilityForTesting() const {
  DCHECK(initialized_);
  boolean is_enabled;
  HRESULT hr = system_media_controls_->get_IsEnabled(&is_enabled);
  DCHECK(SUCCEEDED(hr));
  return is_enabled;
}

void SystemMediaControlsWin::OnPlay() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPlay(this);
}

void SystemMediaControlsWin::OnPause() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPause(this);
}

void SystemMediaControlsWin::OnNext() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnNext(this);
}

void SystemMediaControlsWin::OnPrevious() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPrevious(this);
}

void SystemMediaControlsWin::OnStop() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnStop(this);
}

void SystemMediaControlsWin::OnSeekTo(const base::TimeDelta& time) {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnSeekTo(this, time);
}

ABI::Windows::Media::MediaPlaybackStatus
SystemMediaControlsWin::GetSmtcPlaybackStatus(PlaybackStatus status) {
  switch (status) {
    case PlaybackStatus::kPlaying:
      return ABI::Windows::Media::MediaPlaybackStatus::
          MediaPlaybackStatus_Playing;
    case PlaybackStatus::kPaused:
      return ABI::Windows::Media::MediaPlaybackStatus::
          MediaPlaybackStatus_Paused;
    case PlaybackStatus::kStopped:
      return ABI::Windows::Media::MediaPlaybackStatus::
          MediaPlaybackStatus_Stopped;
  }
  NOTREACHED_IN_MIGRATION();
  return ABI::Windows::Media::MediaPlaybackStatus::MediaPlaybackStatus_Stopped;
}

HRESULT SystemMediaControlsWin::ButtonPressed(
    ISystemMediaTransportControls* sender,
    ISystemMediaTransportControlsButtonPressedEventArgs* args) {
  SystemMediaTransportControlsButton button;
  HRESULT hr = args->get_Button(&button);
  if (FAILED(hr))
    return hr;

  switch (button) {
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Play:
      OnPlay();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Pause:
      OnPause();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Next:
      OnNext();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Previous:
      OnPrevious();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Stop:
      OnStop();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Record:
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_FastForward:
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Rewind:
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_ChannelUp:
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_ChannelDown:
      break;
  }

  return S_OK;
}

HRESULT SystemMediaControlsWin::PlaybackPositionChangeRequested(
    ISystemMediaTransportControls* sender,
    IPlaybackPositionChangeRequestedEventArgs* args) {
  ABI::Windows::Foundation::TimeSpan position;
  HRESULT hr = args->get_RequestedPlaybackPosition(&position);
  if (FAILED(hr))
    return hr;

  OnSeekTo(base::TimeDelta::FromWinrtTimeSpan(position));

  return S_OK;
}

void SystemMediaControlsWin::OnEnabledStatusChangedForTesting() {
  if (g_on_visibility_changed_for_testing_callback) {
    g_on_visibility_changed_for_testing_callback->Run(
        GetVisibilityForTesting());
  }
}

}  // namespace internal

}  // namespace system_media_controls
