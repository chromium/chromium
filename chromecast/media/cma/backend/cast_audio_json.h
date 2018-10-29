// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "base/values.h"

namespace base {
class FilePathWatcher;
class SequencedTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {

class CastAudioJson {
 public:
  // Returns GetFilePathForTuning() if a file exists at that path, otherwise
  // returns GetReadOnlyFilePath().
  static base::FilePath GetFilePath();
  static base::FilePath GetReadOnlyFilePath();
  static base::FilePath GetFilePathForTuning();
};

// Provides an interface for reading CastAudioJson and registering for file
// updates.
class CastAudioJsonProvider {
 public:
  using TuningChangedCallback =
      base::RepeatingCallback<void(std::unique_ptr<base::Value> contents)>;

  virtual ~CastAudioJsonProvider() = default;

  // Returns the contents of cast_audio.json.
  // If a file exists at CastAudioJson::GetFilePathForTuning() and is valid
  // JSON, its contents will be returned. Otherwise, the contents of the file
  // at CastAudioJson::GetReadOnlyFilePath() will be returned.
  // This function will run on the thread on which it is called, and may
  // perform blocking I/O.
  virtual std::unique_ptr<base::Value> GetCastAudioConfig() = 0;

  // |callback| will be called when a new cast_audio config is available.
  // |callback| will always be called from the same thread, but not the same
  // thread on which |SetTuningChangedCallback| is called.
  // |callback| will never be called after ~CastAudioJsonProvider() is called.
  virtual void SetTuningChangedCallback(TuningChangedCallback callback) = 0;
};

class CastAudioJsonProviderImpl : public CastAudioJsonProvider {
 public:
  CastAudioJsonProviderImpl();
  ~CastAudioJsonProviderImpl() override;

 private:
  // CastAudioJsonProvider implementation:
  std::unique_ptr<base::Value> GetCastAudioConfig() override;
  void SetTuningChangedCallback(TuningChangedCallback callback) override;

  void StopWatchingFileOnThread();
  void OnTuningFileChanged(const base::FilePath& path, bool error);

  TuningChangedCallback callback_;
  base::Thread thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::FilePathWatcher> cast_audio_watcher_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioJsonProviderImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_
