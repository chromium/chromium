// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_media_controls/win/system_media_controls_win.h"

#include <systemmediatransportcontrolsinterop.h>
#include <windows.media.control.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include "base/strings/string_piece.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "components/system_media_controls/system_media_controls_observer.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace system_media_controls {

// static
SystemMediaControls* SystemMediaControls::GetInstance() {
  internal::SystemMediaControlsWin* service =
      internal::SystemMediaControlsWin::GetInstance();
  if (service->Initialize())
    return service;
  return nullptr;
}

namespace internal {

using ABI::Windows::Media::ISystemMediaTransportControls;
using ABI::Windows::Media::ISystemMediaTransportControlsButtonPressedEventArgs;
using ABI::Windows::Media::SystemMediaTransportControls;
using ABI::Windows::Media::SystemMediaTransportControlsButton;
using ABI::Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs;
using ABI::Windows::Storage::Streams::IDataWriter;
using ABI::Windows::Storage::Streams::IDataWriterFactory;
using ABI::Windows::Storage::Streams::IOutputStream;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using ABI::Windows::Storage::Streams::IRandomAccessStreamReference;
using ABI::Windows::Storage::Streams::IRandomAccessStreamReferenceStatics;

// static
SystemMediaControlsWin* SystemMediaControlsWin::GetInstance() {
  // We use a base::Singleton here instead of a base::NoDestruct so that we can
  // clean up external listeners against the Windows platform at exit.
  return base::Singleton<SystemMediaControlsWin>::get();
}

SystemMediaControlsWin::SystemMediaControlsWin() = default;

SystemMediaControlsWin::~SystemMediaControlsWin() {
  if (has_valid_registration_token_) {
    DCHECK(system_media_controls_);
    system_media_controls_->remove_ButtonPressed(registration_token_);
    ClearMetadata();
  }
}

bool SystemMediaControlsWin::Initialize() {
  if (attempted_to_initialize_)
    return initialized_;

  attempted_to_initialize_ = true;

  if (!base::win::ResolveCoreWinRTDelayload() ||
      !base::win::ScopedHString::ResolveCoreWinRTStringDelayload()) {
    return false;
  }

  Microsoft::WRL::ComPtr<ISystemMediaTransportControlsInterop> interop;
  HRESULT hr = base::win::GetActivationFactory<
      ISystemMediaTransportControlsInterop,
      RuntimeClass_Windows_Media_SystemMediaTransportControls>(&interop);
  if (FAILED(hr))
    return false;

  hr = interop->GetForWindow(gfx::SingletonHwnd::GetInstance()->hwnd(),
                             IID_PPV_ARGS(&system_media_controls_));
  if (FAILED(hr))
    return false;

  auto handler =
      Microsoft::WRL::Callback<ABI::Windows::Foundation::ITypedEventHandler<
          SystemMediaTransportControls*,
          SystemMediaTransportControlsButtonPressedEventArgs*>>(
          &SystemMediaControlsWin::ButtonPressed);
  hr = system_media_controls_->add_ButtonPressed(handler.Get(),
                                                 &registration_token_);
  if (FAILED(hr))
    return false;

  has_valid_registration_token_ = true;

  hr = system_media_controls_->put_IsEnabled(true);
  if (FAILED(hr))
    return false;

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

void SystemMediaControlsWin::SetPlaybackStatus(PlaybackStatus status) {
  DCHECK(initialized_);
  HRESULT hr =
      system_media_controls_->put_PlaybackStatus(GetSmtcPlaybackStatus(status));
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetTitle(const base::string16& title) {
  DCHECK(initialized_);
  DCHECK(display_properties_);
  base::win::ScopedHString h_title = base::win::ScopedHString::Create(title);
  HRESULT hr = display_properties_->put_Title(h_title.get());
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::SetArtist(const base::string16& artist) {
  DCHECK(initialized_);
  DCHECK(display_properties_);
  base::win::ScopedHString h_artist = base::win::ScopedHString::Create(artist);
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
          HRESULT hr = base::win::GetActivationFactory<
              IRandomAccessStreamReferenceStatics,
              RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference>(
              &reference_statics);
          DCHECK(SUCCEEDED(hr));

          hr = reference_statics->CreateFromStream(icon_stream_.Get(),
                                                   &icon_stream_reference_);
          DCHECK(SUCCEEDED(hr));

          hr = display_updater_->put_Thumbnail(icon_stream_reference_.Get());
          DCHECK(SUCCEEDED(hr));

          hr = display_updater_->Update();
          DCHECK(SUCCEEDED(hr));
        }
        return hr;
      });

  hr = store_async_operation->put_Completed(store_async_callback.Get());
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
}

void SystemMediaControlsWin::UpdateDisplay() {
  DCHECK(initialized_);
  DCHECK(system_media_controls_);
  DCHECK(display_updater_);
  HRESULT hr = system_media_controls_->put_IsEnabled(true);
  DCHECK(SUCCEEDED(hr));

  // |ClearAll()| unsets the type, if we don't set it again then the artist
  // won't be displayed.
  hr = display_updater_->put_Type(
      ABI::Windows::Media::MediaPlaybackType::MediaPlaybackType_Music);
  DCHECK(SUCCEEDED(hr));

  hr = display_updater_->Update();
  DCHECK(SUCCEEDED(hr));
}

void SystemMediaControlsWin::OnPlay() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPlay();
}

void SystemMediaControlsWin::OnPause() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPause();
}

void SystemMediaControlsWin::OnNext() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnNext();
}

void SystemMediaControlsWin::OnPrevious() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnPrevious();
}

void SystemMediaControlsWin::OnStop() {
  for (SystemMediaControlsObserver& obs : observers_)
    obs.OnStop();
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
  NOTREACHED();
  return ABI::Windows::Media::MediaPlaybackStatus::MediaPlaybackStatus_Stopped;
}

// static
HRESULT SystemMediaControlsWin::ButtonPressed(
    ISystemMediaTransportControls* sender,
    ISystemMediaTransportControlsButtonPressedEventArgs* args) {
  SystemMediaTransportControlsButton button;
  HRESULT hr = args->get_Button(&button);
  if (FAILED(hr))
    return hr;

  SystemMediaControlsWin* impl = GetInstance();

  switch (button) {
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Play:
      impl->OnPlay();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Pause:
      impl->OnPause();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Next:
      impl->OnNext();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Previous:
      impl->OnPrevious();
      break;
    case SystemMediaTransportControlsButton::
        SystemMediaTransportControlsButton_Stop:
      impl->OnStop();
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

}  // namespace internal

}  // namespace system_media_controls
