// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/cast_audio_json.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"

namespace chromecast {
namespace media {

namespace {

void ReadFileRunCallback(CastAudioJsonProvider::TuningChangedCallback callback,
                         const base::FilePath& path,
                         bool error) {
  DCHECK(callback);

  std::string contents;
  base::ReadFileToString(path, &contents);
  std::optional<base::Value> value = base::JSONReader::Read(contents);
  if (value && value->is_dict()) {
    callback.Run(std::move(*value).TakeDict());
    return;
  }
  LOG(ERROR) << "Unable to parse JSON in " << path;
}

}  // namespace

#if BUILDFLAG(IS_FUCHSIA)
const char kCastAudioJsonFilePath[] = "/system/data/cast_audio.json";
#else
const char kCastAudioJsonFilePath[] = "/etc/cast_audio.json";
#endif
const char kCastAudioJsonFileName[] = "cast_audio.json";

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

CastAudioJsonProviderImpl::CastAudioJsonProviderImpl() {
  if (base::ThreadPoolInstance::Get()) {
    cast_audio_watcher_ = base::SequenceBound<FileWatcher>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::LOWEST}));
  }
}

CastAudioJsonProviderImpl::~CastAudioJsonProviderImpl() = default;

std::optional<base::Value::Dict>
CastAudioJsonProviderImpl::GetCastAudioConfig() {
  std::string contents;
  base::ReadFileToString(CastAudioJson::GetFilePath(), &contents);
  std::optional<base::Value> value = base::JSONReader::Read(contents);
  if (!value || value->is_dict()) {
    return std::nullopt;
  }

  return std::move(*value).TakeDict();
}

void CastAudioJsonProviderImpl::SetTuningChangedCallback(
    TuningChangedCallback callback) {
  if (cast_audio_watcher_) {
    cast_audio_watcher_.AsyncCall(&FileWatcher::SetTuningChangedCallback)
        .WithArgs(std::move(callback));
  }
}

CastAudioJsonProviderImpl::FileWatcher::FileWatcher() = default;
CastAudioJsonProviderImpl::FileWatcher::~FileWatcher() = default;

void CastAudioJsonProviderImpl::FileWatcher::SetTuningChangedCallback(
    TuningChangedCallback callback) {
  watcher_.Watch(
      CastAudioJson::GetFilePathForTuning(),
      base::FilePathWatcher::Type::kNonRecursive,
      base::BindRepeating(&ReadFileRunCallback, std::move(callback)));
}

}  // namespace media
}  // namespace chromecast
