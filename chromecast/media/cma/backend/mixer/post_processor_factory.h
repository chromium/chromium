// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSOR_FACTORY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSOR_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

namespace base {
class FilePath;
class ScopedNativeLibrary;
}  // namespace base

namespace chromecast {
namespace media {

class AudioPostProcessor2;

class PostProcessorFactory {
 public:
  PostProcessorFactory();

  PostProcessorFactory(const PostProcessorFactory&) = delete;
  PostProcessorFactory& operator=(const PostProcessorFactory&) = delete;

  ~PostProcessorFactory();

  // Checks if a library is a V1 or V2 post processor.
  static bool IsPostProcessorLibrary(const base::FilePath& library_path);

  // Creates an instance of AudioPostProcessor2 or a wrapped AudioPostProcessor.
  // By default, will attempt to find the library in
  // /system/chrome/lib/processors/|library_name|. Will fall back to
  // searching for /oem_cast_shlib/processors/|library_name|, and finally
  // searching for |library_name| in LD_LIBRARY_PATH.
  std::unique_ptr<AudioPostProcessor2> CreatePostProcessor(
      const std::string& library_name,
      const std::string& config,
      int channels);

 private:
  // Contains all libraries in use;
  // Functions in shared objects cannot be used once library is closed.
  std::vector<std::unique_ptr<base::ScopedNativeLibrary>> libraries_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSOR_FACTORY_H_
