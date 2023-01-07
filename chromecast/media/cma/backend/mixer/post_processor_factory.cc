// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processor_factory.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_native_library.h"
#include "chromecast/media/audio/audio_log.h"
#include "chromecast/media/cma/backend/mixer/post_processor_paths.h"
#include "chromecast/media/cma/backend/mixer/post_processor_registry.h"
#include "chromecast/media/cma/backend/mixer/post_processors/post_processor_wrapper.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"
#include "chromecast/public/media/audio_post_processor_shlib.h"

namespace chromecast {
namespace media {

namespace {

const char kV1SoCreateFunction[] = "AudioPostProcessorShlib_Create";
const char kV2SoCreateFunction[] = "AudioPostProcessor2Shlib_Create";
const char kInitLoggingFunction[] = "AudioPostProcessor2Shlib_InitLogging";

base::FilePath FindLibrary(const std::string& library_name) {
  base::FilePath relative_path(library_name);
  base::FilePath full_path = GetPostProcessorDirectory();
  full_path = full_path.Append(relative_path);
  if (base::PathExists(full_path)) {
    return full_path;
  }
  full_path = GetOemPostProcessorDirectory();
  full_path = full_path.Append(relative_path);
  if (base::PathExists(full_path)) {
    return full_path;
  }
  LOG(WARNING) << library_name << " not found in "
               << GetPostProcessorDirectory().AsUTF8Unsafe() << " or "
               << GetOemPostProcessorDirectory().AsUTF8Unsafe()
               << ". Prefer storing post processors in these directories.";
  return relative_path;
}

}  // namespace

using CreatePostProcessor2Function =
    AudioPostProcessor2* (*)(const std::string&, int);

using CreatePostProcessorFunction = AudioPostProcessor* (*)(const std::string&,
                                                            int);

using InitLoggingFunction = void (*)(logging::AudioLogMessage::BufferManager*);

PostProcessorFactory::PostProcessorFactory() = default;
PostProcessorFactory::~PostProcessorFactory() = default;

std::unique_ptr<AudioPostProcessor2> PostProcessorFactory::CreatePostProcessor(
    const std::string& library_name,
    const std::string& config,
    int channels) {
  std::unique_ptr<AudioPostProcessor2> builtin =
      PostProcessorRegistry::Get()->Create(library_name, config, channels);
  if (builtin) {
    LOG(INFO) << "Loaded builtin " << library_name;
    return builtin;
  }

  base::FilePath path = FindLibrary(library_name);
  libraries_.push_back(std::make_unique<base::ScopedNativeLibrary>(path));
  CHECK(libraries_.back()->is_valid())
      << "Could not open post processing library " << path;

  auto init_logging = reinterpret_cast<InitLoggingFunction>(
      libraries_.back()->GetFunctionPointer(kInitLoggingFunction));
  if (init_logging) {
    init_logging(logging::AudioLogMessage::GetBufferManager());
  }

  auto v2_create = reinterpret_cast<CreatePostProcessor2Function>(
      libraries_.back()->GetFunctionPointer(kV2SoCreateFunction));
  if (v2_create) {
    return base::WrapUnique(v2_create(config, channels));
  }

  auto v1_create = reinterpret_cast<CreatePostProcessorFunction>(
      libraries_.back()->GetFunctionPointer(kV1SoCreateFunction));

  DCHECK(v1_create) << "Could not find " << kV1SoCreateFunction << "() in "
                    << library_name;

  LOG(WARNING) << "[Deprecated]: AudioPostProcessor will be deprecated soon."
               << " Please update " << library_name
               << " to AudioPostProcessor2.";

  return std::make_unique<AudioPostProcessorWrapper>(
      base::WrapUnique(v1_create(config, channels)), channels);
}

// static
bool PostProcessorFactory::IsPostProcessorLibrary(
    const base::FilePath& library_path) {
  base::ScopedNativeLibrary library(library_path);
  DCHECK(library.is_valid()) << "Could not open library " << library_path;

  // Check if library is V1 post processor.
  void* v1_create = library.GetFunctionPointer(kV1SoCreateFunction);
  if (v1_create) {
    return true;
  }

  // Check if library is V2 post processor.
  void* v2_create = library.GetFunctionPointer(kV2SoCreateFunction);
  if (v2_create) {
    return true;
  }

  return false;
}

}  // namespace media
}  // namespace chromecast
