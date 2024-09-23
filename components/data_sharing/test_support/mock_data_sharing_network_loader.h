// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_DATA_SHARING_NETWORK_LOADER_H_
#define COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_DATA_SHARING_NETWORK_LOADER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace data_sharing {

// The mock implementation of the DataSharingNetworkLoader.
class MockDataSharingNetworkLoader : public DataSharingNetworkLoader {
 public:
  MockDataSharingNetworkLoader();
  ~MockDataSharingNetworkLoader() override;

  // Disallow copy/assign.
  MockDataSharingNetworkLoader(const MockDataSharingNetworkLoader&) = delete;
  MockDataSharingNetworkLoader& operator=(const MockDataSharingNetworkLoader&) =
      delete;

  // DataSharingNetworkLoader Impl.
  MOCK_METHOD5(LoadUrl,
               void(const GURL&,
                    const std::vector<std::string>&,
                    const std::string&,
                    const net::NetworkTrafficAnnotationTag&,
                    NetworkLoaderCallback));
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_DATA_SHARING_NETWORK_LOADER_H_
