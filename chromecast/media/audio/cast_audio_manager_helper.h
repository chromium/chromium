// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_HELPER_H_
#define CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_HELPER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class AudioManagerBase;
}  // namespace media

namespace chromecast {
namespace media {

class CmaBackendFactory;

// Helper class to share common logic between different AudioManager.
class CastAudioManagerHelper {
 public:
  class Delegate {
   public:
    // Get session ID for the provided group ID.
    virtual std::string GetSessionId(const std::string& group_id) = 0;
    // Get whether the session is audio-only for the provided session ID.
    virtual bool IsAudioOnlySession(const std::string& session_id) = 0;
    // Get whether the session is launched in a group for the provided session
    // ID.
    virtual bool IsGroup(const std::string& session_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  CastAudioManagerHelper(
      ::media::AudioManagerBase* audio_manager,
      Delegate* delegate,
      base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      external_service_support::ExternalConnector* connector);
  ~CastAudioManagerHelper();

  ::media::AudioManagerBase* audio_manager() { return audio_manager_; }

  base::SingleThreadTaskRunner* media_task_runner() {
    return media_task_runner_.get();
  }

  CmaBackendFactory* GetCmaBackendFactory();
  std::string GetSessionId(const std::string& audio_group_id);
  bool IsAudioOnlySession(const std::string& session_id);
  bool IsGroup(const std::string& session_id);

  external_service_support::ExternalConnector* GetConnector();

 private:
  ::media::AudioManagerBase* const audio_manager_;
  Delegate* const delegate_;
  base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter_;
  CmaBackendFactory* cma_backend_factory_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // |audio_thread_connector_| is created on the main thread, but is used on the
  // Audio thread.
  std::unique_ptr<external_service_support::ExternalConnector>
      audio_thread_connector_;

  CastAudioManagerHelper(const CastAudioManagerHelper&) = delete;
  CastAudioManagerHelper& operator=(const CastAudioManagerHelper&) = delete;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAST_AUDIO_MANAGER_HELPER_H_
