// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_CMA_BACKEND_FACTORY_H_
#define CHROMECAST_MEDIA_API_CMA_BACKEND_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"

namespace chromecast {
namespace media {

class CmaBackend;
class MediaPipelineBackendManager;
struct MediaPipelineDeviceParams;

// Abstract base class to create CmaBackend.
class CmaBackendFactory {
 public:
  static std::unique_ptr<CmaBackendFactory> Create(
      MediaPipelineBackendManager* media_pipeline_backend_manager,
      std::unique_ptr<external_service_support::ExternalConnector> connector);

  virtual ~CmaBackendFactory() = default;

  // Creates a CMA backend. Must be called on the same thread as
  // |media_task_runner_|.
  virtual std::unique_ptr<CmaBackend> CreateBackend(
      const MediaPipelineDeviceParams& params) = 0;

  // Returns |media_task_runner_|.
  virtual scoped_refptr<base::SequencedTaskRunner> GetMediaTaskRunner() = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_CMA_BACKEND_FACTORY_H_
