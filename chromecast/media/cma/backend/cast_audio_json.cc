// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/cast_audio_json.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"

namespace chromecast {
namespace media {

#if defined(OS_FUCHSIA)
const char kCastAudioJsonFilePath[] = "/system/data/cast_audio.json";
#else
const char kCastAudioJsonFilePath[] = "/etc/cast_audio.json";
#endif
const char kCastAudioJsonFileName[] = "cast_audio.json";

#define ENSURE_OWN_THREAD(method, ...)                                     \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                       \
    task_runner_->PostTask(                                                \
        FROM_HERE, base::BindOnce(&CastAudioJsonProviderImpl::method,      \
                                  base::Unretained(this), ##__VA_ARGS__)); \
    return;                                                                \
  }

// static
base::FilePath CastAudioJson::GetFilePath() {
  base::FilePath tuning_path = CastAudioJson::GetFilePathForTuning();
  if (base::PathExists(tuning_path)) {
    return tuning_path;
  }

  return CastAudioJson::GetReadOnlyFilePath();
}

// static
base::FilePath CastAudioJson::GetReadOnlyFilePath() {
  return base::FilePath(kCastAudioJsonFilePath);
}

// static
base::FilePath CastAudioJson::GetFilePathForTuning() {
  return base::GetHomeDir().Append(kCastAudioJsonFileName);
}

CastAudioJsonProviderImpl::CastAudioJsonProviderImpl()
    : thread_("cast_audio_json_provider"),
      cast_audio_watcher_(std::make_unique<base::FilePathWatcher>()) {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread_.StartWithOptions(options);
  task_runner_ = thread_.task_runner();
}

CastAudioJsonProviderImpl::~CastAudioJsonProviderImpl() {
  StopWatchingFileOnThread();
}

std::unique_ptr<base::Value> CastAudioJsonProviderImpl::GetCastAudioConfig() {
  std::string contents;
  base::ReadFileToString(CastAudioJson::GetFilePath(), &contents);
  return base::JSONReader::Read(contents);
}

void CastAudioJsonProviderImpl::SetTuningChangedCallback(
    TuningChangedCallback callback) {
  ENSURE_OWN_THREAD(SetTuningChangedCallback, std::move(callback));

  CHECK(!callback_);
  callback_ = callback;
  cast_audio_watcher_->Watch(
      CastAudioJson::GetFilePathForTuning(), false /* recursive */,
      base::BindRepeating(&CastAudioJsonProviderImpl::OnTuningFileChanged,
                          base::Unretained(this)));
}

void CastAudioJsonProviderImpl::StopWatchingFileOnThread() {
  ENSURE_OWN_THREAD(StopWatchingFileOnThread);
  cast_audio_watcher_.reset();
}

void CastAudioJsonProviderImpl::OnTuningFileChanged(const base::FilePath& path,
                                                    bool error) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(callback_);

  std::string contents;
  base::ReadFileToString(path, &contents);
  std::unique_ptr<base::Value> value = base::JSONReader::Read(contents);
  if (value) {
    callback_.Run(std::move(value));
    return;
  }
  LOG(ERROR) << "Unable to parse JSON in " << path;
}

}  // namespace media
}  // namespace chromecast
