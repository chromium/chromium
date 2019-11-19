// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "net/base/request_priority.h"
#include "net/nqe/effective_connection_type.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

class DataReductionProxyDataTest : public testing::Test {
 public:
  DataReductionProxyDataTest() {}

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
};

TEST_F(DataReductionProxyDataTest, BasicSettersAndGetters) {
  std::unique_ptr<DataReductionProxyData> data(new DataReductionProxyData());
  EXPECT_FALSE(data->used_data_reduction_proxy());
  data->set_used_data_reduction_proxy(true);
  EXPECT_TRUE(data->used_data_reduction_proxy());
  data->set_used_data_reduction_proxy(false);
  EXPECT_FALSE(data->used_data_reduction_proxy());

  EXPECT_FALSE(data->lite_page_received());
  data->set_lite_page_received(true);
  EXPECT_TRUE(data->lite_page_received());
  data->set_lite_page_received(false);
  EXPECT_FALSE(data->lite_page_received());

  EXPECT_FALSE(data->black_listed());
  data->set_black_listed(true);
  EXPECT_TRUE(data->black_listed());
  data->set_black_listed(false);
  EXPECT_FALSE(data->black_listed());

  EXPECT_EQ(std::string(), data->session_key());
  std::string session_key = "test-key";
  data->set_session_key(session_key);
  EXPECT_EQ(session_key, data->session_key());

  EXPECT_EQ(GURL(std::string()), data->request_url());
  GURL test_url("test-url");
  data->set_request_url(test_url);
  EXPECT_EQ(test_url, data->request_url());

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            data->effective_connection_type());
  data->set_effective_connection_type(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
            data->effective_connection_type());

  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
            data->connection_type());
  data->set_connection_type(net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_WIFI,
            data->connection_type());

  EXPECT_FALSE(data->page_id());
  uint64_t page_id = 1;
  data->set_page_id(page_id);
  EXPECT_EQ(page_id, data->page_id().value());
}

TEST_F(DataReductionProxyDataTest, DeepCopy) {
  const struct {
    bool data_reduction_used;
    bool lite_page_test_value;
  } tests[] = {
      {
          false, true,
      },
      {
          false, false,
      },
      {
          true, false,
      },
      {
          true, true,
      },
  };

  for (size_t i = 0; i < base::size(tests); ++i) {
    static const char kSessionKey[] = "test-key";
    static const GURL kTestURL("test-url");
    std::unique_ptr<DataReductionProxyData> data(new DataReductionProxyData());
    data->set_used_data_reduction_proxy(tests[i].data_reduction_used);
    data->set_lite_page_received(tests[i].lite_page_test_value);
    data->set_black_listed(tests[i].lite_page_test_value);
    data->set_session_key(kSessionKey);
    data->set_request_url(kTestURL);
    data->set_effective_connection_type(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
    data->set_connection_type(net::NetworkChangeNotifier::CONNECTION_WIFI);
    data->set_page_id(2u);
    std::unique_ptr<DataReductionProxyData> copy = data->DeepCopy();
    EXPECT_EQ(tests[i].lite_page_test_value, copy->lite_page_received());
    EXPECT_EQ(tests[i].lite_page_test_value, copy->black_listed());
    EXPECT_EQ(tests[i].data_reduction_used, copy->used_data_reduction_proxy());
    EXPECT_EQ(kSessionKey, copy->session_key());
    EXPECT_EQ(kTestURL, copy->request_url());
    EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
              copy->effective_connection_type());
    EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_WIFI,
              copy->connection_type());
    EXPECT_EQ(2u, data->page_id().value());
  }
}

}  // namespace

}  // namespace data_reduction_proxy
