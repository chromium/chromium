// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/client_set.h"

#include "base/ranges/algorithm.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/test/empty_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

TEST(DownloadServiceClientSetTest, TestGetClient) {
  auto client = std::make_unique<test::EmptyClient>();
  Client* raw_client = client.get();

  auto client_map = std::make_unique<DownloadClientMap>();
  client_map->insert(std::make_pair(DownloadClient::TEST, std::move(client)));
  ClientSet clients(std::move(client_map));

  EXPECT_EQ(raw_client, clients.GetClient(DownloadClient::TEST));
  EXPECT_EQ(nullptr, clients.GetClient(DownloadClient::INVALID));
}

TEST(DownloadServiceClientSetTest, TestGetRegisteredClients) {
  auto client = std::make_unique<test::EmptyClient>();

  auto client_map = std::make_unique<DownloadClientMap>();
  client_map->insert(std::make_pair(DownloadClient::TEST, std::move(client)));
  ClientSet clients(std::move(client_map));

  std::set<DownloadClient> expected_set = {DownloadClient::TEST,
                                           DownloadClient::DEBUGGING};
  std::set<DownloadClient> actual_set = clients.GetRegisteredClients();

  EXPECT_TRUE(base::ranges::equal(expected_set, actual_set));
}

}  // namespace download
