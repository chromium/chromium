// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/video_tutorials/video_tutorial_service.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace video_tutorials {

class VideoTutorialService;

// A factory to create one unique VideoTutorialService.
class VideoTutorialServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static VideoTutorialServiceFactory* GetInstance();
  static VideoTutorialService* GetForKey(SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<VideoTutorialServiceFactory>;

  VideoTutorialServiceFactory();
  ~VideoTutorialServiceFactory() override = default;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_VIDEO_TUTORIAL_SERVICE_FACTORY_H_
