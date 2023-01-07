// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_VOLUME_CONTROL_ANDROID_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_VOLUME_CONTROL_ANDROID_H_

#include <map>
#include <vector>

#include "base/android/jni_android.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chromecast/media/cma/backend/android/audio_sink_manager.h"
#include "chromecast/media/cma/backend/android/volume_cache.h"

namespace chromecast {
namespace media {

class VolumeControlAndroid : SystemVolumeTableAccessApi {
 public:
  VolumeControlAndroid();

  VolumeControlAndroid(const VolumeControlAndroid&) = delete;
  VolumeControlAndroid& operator=(const VolumeControlAndroid&) = delete;

  ~VolumeControlAndroid() override;

  void AddVolumeObserver(VolumeObserver* observer);
  void RemoveVolumeObserver(VolumeObserver* observer);
  float GetVolume(AudioContentType type);
  void SetVolume(VolumeChangeSource source, AudioContentType type, float level);
  bool IsMuted(AudioContentType type);
  void SetMuted(VolumeChangeSource source, AudioContentType type, bool muted);
  void SetOutputLimit(AudioContentType type, float limit);
  float VolumeToDbFSCached(AudioContentType type, float volume);
  float DbFSToVolumeCached(AudioContentType type, float db);

  // Called from java to signal a change volume.
  void OnVolumeChange(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jint type,
                      jfloat level);

  void OnMuteChange(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint type,
                    jboolean muted);

  // SystemVolumeTableAccessApi implementation.
  int GetMaxVolumeIndex(AudioContentType type) override;
  float VolumeToDbFS(AudioContentType type, float volume) override;

 private:
  void InitializeOnThread();
  void SetVolumeOnThread(VolumeChangeSource source,
                         AudioContentType type,
                         float level,
                         bool from_android);
  void SetMutedOnThread(VolumeChangeSource source,
                        AudioContentType type,
                        bool muted,
                        bool from_android);
  void ReportVolumeChangeOnThread(AudioContentType type, float level);
  void ReportMuteChangeOnThread(AudioContentType type, bool muted);

  // For the user of the VolumeControl, all volume values are in the volume
  // table domain of kMedia (MUSIC). For volume types other than media,
  // VolumeControl converts them internally into their proper volume table
  // domains.
  float MapIntoDifferentVolumeTableDomain(AudioContentType from_type,
                                          AudioContentType to_type,
                                          float level);

  const bool is_single_volume_;
  base::android::ScopedJavaGlobalRef<jobject> j_volume_control_;

  std::map<AudioContentType, std::unique_ptr<VolumeCache>> volume_cache_;

  base::Lock volume_lock_;
  std::map<AudioContentType, float> volumes_;
  std::map<AudioContentType, bool> muted_;

  base::Lock observer_lock_;
  std::vector<VolumeObserver*> volume_observers_;

  base::Thread thread_;
  base::WaitableEvent initialize_complete_event_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_VOLUME_CONTROL_ANDROID_H_
