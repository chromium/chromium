// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_TEST_MOCK_CMA_BACKEND_FACTORY_H_
#define CHROMECAST_MEDIA_API_TEST_MOCK_CMA_BACKEND_FACTORY_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/api/cma_backend_factory.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace media {

class MockCmaBackendFactory : public CmaBackendFactory {
 public:
  MockCmaBackendFactory();
  ~MockCmaBackendFactory() override;

  MOCK_METHOD(std::unique_ptr<CmaBackend>, CreateBackend,
              (const MediaPipelineDeviceParams&), (override));
  MOCK_METHOD(scoped_refptr<base::SequencedTaskRunner>, GetMediaTaskRunner,
              (), (override));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_TEST_MOCK_CMA_BACKEND_FACTORY_H_
