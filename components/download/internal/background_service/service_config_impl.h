// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SERVICE_CONFIG_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SERVICE_CONFIG_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/download/public/background_service/service_config.h"

namespace download {

struct Configuration;

class ServiceConfigImpl : public ServiceConfig {
 public:
  explicit ServiceConfigImpl(Configuration* config);

  ServiceConfigImpl(const ServiceConfigImpl&) = delete;
  ServiceConfigImpl& operator=(const ServiceConfigImpl&) = delete;

  ~ServiceConfigImpl() override;

  // ServiceConfig implementation.
  uint32_t GetMaxScheduledDownloadsPerClient() const override;
  uint32_t GetMaxConcurrentDownloads() const override;
  const base::TimeDelta& GetFileKeepAliveTime() const override;

 private:
  raw_ptr<struct Configuration> config_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SERVICE_CONFIG_IMPL_H_
