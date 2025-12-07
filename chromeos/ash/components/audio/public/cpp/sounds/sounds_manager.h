// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_PUBLIC_CPP_SOUNDS_SOUNDS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_PUBLIC_CPP_SOUNDS_SOUNDS_MANAGER_H_

#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_export.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace audio {

// This class is used for reproduction of system sounds. All methods
// should be accessed from the Audio thread.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO_PUBLIC_CPP_SOUNDS)
    SoundsManager {
 public:
  typedef int SoundKey;

  // Creates a singleton instance of the SoundsManager.
  using StreamFactoryBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory>)>;
  static void Create(StreamFactoryBinder stream_factory_binder);

  // Removes a singleton instance of the SoundsManager.
  static void Shutdown();

  // Returns a pointer to a singleton instance of the SoundsManager.
  static SoundsManager* Get();

  SoundsManager(const SoundsManager&) = delete;
  SoundsManager& operator=(const SoundsManager&) = delete;

  // Initializes sounds manager for testing. The |manager| will be owned
  // by the internal pointer and will be deleted by Shutdown().
  static void InitializeForTesting(SoundsManager* manager);

  // Initializes SoundsManager with the wav data or the flac data for the system
  // sounds. The `codec` should be `kPCM` for the wav audio data or `kFLAC` for
  // the flac audio data. Returns true if SoundsManager was successfully
  // initialized.
  virtual bool Initialize(SoundKey key,
                          std::string_view data,
                          media::AudioCodec codec) = 0;

  // Plays sound identified by |key|, returns false if SoundsManager
  // was not properly initialized.
  virtual bool Play(SoundKey key) = 0;

  // Stops playing sound identified by |key|, returns false if SoundsManager
  // was not properly initialized.
  virtual bool Stop(SoundKey key) = 0;

  // Returns duration of the sound identified by |key|. If SoundsManager
  // was not properly initialized or |key| was not registered, this
  // method returns an empty value.
  virtual base::TimeDelta GetDuration(SoundKey key) = 0;

 protected:
  SoundsManager();
  virtual ~SoundsManager();

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace audio

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_PUBLIC_CPP_SOUNDS_SOUNDS_MANAGER_H_
