// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/tutorial_factory_helper.h"

#include <utility>

#include "chrome/browser/video_tutorials/internal/tutorial_service_impl.h"

namespace video_tutorials {

std::unique_ptr<VideoTutorialService> CreateVideoTutorialService() {
  // TODO(shaktisahu): Pass correct values.
  return std::make_unique<TutorialServiceImpl>(nullptr, nullptr, nullptr);
}

}  // namespace video_tutorials
