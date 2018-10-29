// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/media/audio/mixer_service/mixer_service_connection_factory.h"
#include "media/audio/audio_manager_base.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromecast {

namespace media {

class CastAudioMixer;
class CastAudioManagerTest;
class CastAudioOutputStreamTest;
class CmaBackendFactory;

class CastAudioManager : public ::media::AudioManagerBase {
 public:
  CastAudioManager(
      std::unique_ptr<::media::AudioThread> audio_thread,
      ::media::AudioLogFactory* audio_log_factory,
      base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
      scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      service_manager::Connector* connector,
      bool use_mixer);
  ~CastAudioManager() override;

  // AudioManagerBase implementation.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(
      ::media::AudioDeviceNames* device_names) override;
  ::media::AudioParameters GetInputStreamParameters(
      const std::string& device_id) override;
  const char* GetName() override;
  void ReleaseOutputStream(::media::AudioOutputStream* stream) override;

  CmaBackendFactory* cma_backend_factory();
  base::SingleThreadTaskRunner* media_task_runner() {
    return media_task_runner_.get();
  }

  void SetConnectorForTesting(
      std::unique_ptr<service_manager::Connector> connector);

 protected:
  // AudioManagerBase implementation.
  ::media::AudioOutputStream* MakeLinearOutputStream(
      const ::media::AudioParameters& params,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioOutputStream* MakeLowLatencyOutputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioInputStream* MakeLinearInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioInputStream* MakeLowLatencyInputStream(
      const ::media::AudioParameters& params,
      const std::string& device_id,
      const ::media::AudioManager::LogCallback& log_callback) override;
  ::media::AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const ::media::AudioParameters& input_params) override;

  // Generates a CastAudioOutputStream for |mixer_|.
  virtual ::media::AudioOutputStream* MakeMixerOutputStream(
      const ::media::AudioParameters& params);

 private:
  friend class CastAudioMixer;
  friend class CastAudioManagerTest;
  friend class CastAudioOutputStreamTest;
  service_manager::Connector* GetConnector();
  void BindConnectorRequest(service_manager::mojom::ConnectorRequest request);

  CastAudioManager(
      std::unique_ptr<::media::AudioThread> audio_thread,
      ::media::AudioLogFactory* audio_log_factory,
      base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
      scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      service_manager::Connector* connector,
      bool use_mixer,
      bool force_use_cma_backend_for_output);

  // Return nullptr if it is not appropriate to use MixerServiceConnection
  // for output stream audio playback.
  MixerServiceConnectionFactory*
  GetMixerServiceConnectionFactoryForOutputStream(
      const ::media::AudioParameters& params);

  base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter_;
  CmaBackendFactory* cma_backend_factory_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> browser_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  service_manager::Connector* const browser_connector_;
  std::unique_ptr<::media::AudioOutputStream> mixer_output_stream_;
  std::unique_ptr<CastAudioMixer> mixer_;
  std::unique_ptr<service_manager::Connector> connector_;

  // Let unit test force the CastOutputStream to uses
  // CmaBackend implementation.
  // TODO(b/117980762):: After refactoring CastOutputStream, so
  // that the CastOutputStream has a unified output API, regardless
  // of the platform condition, then the unit test would be able to test
  // CastOutputStream properly.
  bool force_use_cma_backend_for_output_;
  MixerServiceConnectionFactory mixer_service_connection_factory_;

  // Weak pointers must be dereferenced on the |browser_task_runner|.
  base::WeakPtr<CastAudioManager> weak_this_;
  base::WeakPtrFactory<CastAudioManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioManager);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_H_
