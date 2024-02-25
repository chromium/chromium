// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_EMPTY_DATA_SHARING_SERVICE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_EMPTY_DATA_SHARING_SERVICE_H_

#include "components/data_sharing/public/data_sharing_service.h"

namespace data_sharing {

// An empty implementation of DataSharingService that can be used when the
// data sharing feature is disabled.
class EmptyDataSharingService : public DataSharingService {
 public:
  EmptyDataSharingService();
  ~EmptyDataSharingService() override;

  // Disallow copy/assign.
  EmptyDataSharingService(const EmptyDataSharingService&) = delete;
  EmptyDataSharingService& operator=(const EmptyDataSharingService&) = delete;

  // DataSharingService implementation.
  bool IsEmptyService() override;
  DataSharingNetworkLoader* GetDataSharingNetworkLoader() override;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_EMPTY_DATA_SHARING_SERVICE_H_
