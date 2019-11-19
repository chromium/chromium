// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_JOB_FACTORY_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_JOB_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_job.h"
#include "components/download/public/common/url_loader_factory_provider.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace download {
class DownloadItem;

// Factory class to create different kinds of DownloadJob.
class COMPONENTS_DOWNLOAD_EXPORT DownloadJobFactory {
 public:
  static std::unique_ptr<DownloadJob> CreateJob(
      DownloadItem* download_item,
      DownloadJob::CancelRequestCallback cancel_request_callback,
      const DownloadCreateInfo& create_info,
      bool is_save_package_download,
      URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
          url_loader_factory_provider,
      service_manager::Connector* connector);

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadJobFactory);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_JOB_FACTORY_H_
