// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "base/values.h"

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
  // |callback| will always be called from the same thread, but not necessarily
  // the same thread on which |SetTuningChangedCallback| is called.
  virtual void SetTuningChangedCallback(TuningChangedCallback callback) = 0;
};

class CastAudioJsonProviderImpl : public CastAudioJsonProvider {
 public:
  CastAudioJsonProviderImpl();
  ~CastAudioJsonProviderImpl() override;

 private:
  class FileWatcher {
   public:
    FileWatcher();
    ~FileWatcher();

    void SetTuningChangedCallback(TuningChangedCallback callback);

   private:
    base::FilePathWatcher watcher_;
  };

  // CastAudioJsonProvider implementation:
  std::unique_ptr<base::Value> GetCastAudioConfig() override;
  void SetTuningChangedCallback(TuningChangedCallback callback) override;

  base::SequenceBound<FileWatcher> cast_audio_watcher_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioJsonProviderImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_CAST_AUDIO_JSON_H_
