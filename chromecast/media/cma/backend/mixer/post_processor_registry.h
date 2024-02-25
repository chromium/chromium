// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSOR_REGISTRY_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSOR_REGISTRY_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"

namespace chromecast {
namespace media {

// Global registry for built-in postprocessors.
class PostProcessorRegistry {
 public:
  using CreateFunction = base::RepeatingCallback<std::unique_ptr<
      AudioPostProcessor2>(const std::string& config, int channels)>;

  // Returns the global registry instance for the current process.
  static PostProcessorRegistry* Get();

  PostProcessorRegistry();
  ~PostProcessorRegistry();

  PostProcessorRegistry(const PostProcessorRegistry&) = delete;
  PostProcessorRegistry& operator=(const PostProcessorRegistry&) = delete;

  // Registers a function to create a postprocessor for the given
  // |library_name|, which should match the name used for the shlib for that
  // postprocessor.
  void Register(const std::string& library_name,
                CreateFunction create_function);

  // Creates a postprocessor instance for the given |library_name|, if it has
  // been registered. Returns nullptr if no creation function has been
  // registered for that name.
  std::unique_ptr<AudioPostProcessor2> Create(const std::string& library_name,
                                              const std::string& config,
                                              int channels);

  // Returns a map of registered libraries and their create functions.
  const base::flat_map<std::string, CreateFunction>& Libraries() const {
    return creators_;
  }

 private:
  base::flat_map<std::string /* library_name */, CreateFunction> creators_;
};

inline PostProcessorRegistry::PostProcessorRegistry() = default;
inline PostProcessorRegistry::~PostProcessorRegistry() = default;

// Helper class and macro for auto-registering postprocessors with simple
// constructors.
template <typename PostProcessor>
struct PostProcessorCreator {
  PostProcessorCreator(const std::string& library_name) {
    PostProcessorRegistry::Get()->Register(
        library_name, base::BindRepeating(&PostProcessorCreator::Create));
  }

  static std::unique_ptr<AudioPostProcessor2> Create(const std::string& config,
                                                     int channels) {
    return std::make_unique<PostProcessor>(config, channels);
  }
};

#define REGISTER_POSTPROCESSOR(Type, library_name)                       \
  static PostProcessorCreator<Type>* static_postprocessor_registration = \
      new PostProcessorCreator<Type>(library_name)

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSOR_REGISTRY_H_
