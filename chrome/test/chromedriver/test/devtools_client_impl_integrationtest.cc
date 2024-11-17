// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/target_utils.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/chrome/web_view_info.h"
#include "chrome/test/chromedriver/test/integration_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string ToString(const base::Value::Dict& node) {
  std::string json;
  base::JSONWriter::Write(node, &json);
  return json;
}

class DevToolsClientImplTest : public IntegrationTest {
 protected:
  DevToolsClientImplTest() = default;
};

}  // namespace

TEST_F(DevToolsClientImplTest, DeleteGlobalJSON) {
  // During page initialization two frames are created in a row.
  // In this test we want to verity that if the global JSON object was saved in
  // the first frame the saved value will be available in the second frame as
  // well.
  Status status{kOk};
  ASSERT_TRUE(StatusOk(SetUpConnection()));
  Timeout timeout{base::Seconds(60)};
  status = target_utils::WaitForPage(*browser_client_, timeout);
  ASSERT_TRUE(StatusOk(status));
  WebViewsInfo views_info;
  status =
      target_utils::GetWebViewsInfo(*browser_client_, &timeout, views_info);
  ASSERT_TRUE(StatusOk(status));
  const WebViewInfo* view_info = views_info.FindFirst(WebViewInfo::kPage);
  ASSERT_NE(view_info, nullptr);
  std::unique_ptr<DevToolsClient> client;
  status = target_utils::AttachToPageTarget(*browser_client_, view_info->id,
                                            &timeout, client);
  ASSERT_TRUE(StatusOk(status));

  DevToolsClientImpl* browser_client_impl =
      static_cast<DevToolsClientImpl*>(browser_client_.get());
  DevToolsClientImpl* page_client_impl =
      static_cast<DevToolsClientImpl*>(client.get());
  status = page_client_impl->AttachTo(browser_client_impl);
  ASSERT_TRUE(StatusOk(status));

  base::Value::Dict params;
  base::Value::Dict result;
  params.Set(
      "expression",
      "window.page_label_for_test = \"starting\"; window.page_label_for_test");
  status = client->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, &timeout, &result);
  const std::string* value = result.FindStringByDottedPath("result.value");
  ASSERT_THAT(value, ::testing::Pointee(testing::Eq("starting")))
      << ToString(result);
  bool frame_has_changed = false;

  params.clear();
  params.Set("url", "data:,landed");
  status = client->SendCommandAndIgnoreResponse("Page.navigate", params);
  ASSERT_TRUE(StatusOk(status));

  Timeout navigation_timeout(base::Seconds(10), &timeout);
  while (!frame_has_changed && !navigation_timeout.IsExpired()) {
    params.clear();
    result.clear();
    params.Set("expression", "window.page_label_for_test");
    status = client->SendCommandAndGetResultWithTimeout(
        "Runtime.evaluate", params, &navigation_timeout, &result);
    if (status.code() == kTimeout) {
      break;
    }

    ASSERT_TRUE(StatusOk(status));
    value = result.FindStringByDottedPath("result.value");
    frame_has_changed = value == nullptr;
  }

  if (frame_has_changed) {
    // frame_has_changed is only true if the value of window.page_label_for_test
    // was set in the first frame and the page has switched to the second frame.
    // This means that the global JSON was saved in the first frame as well.
    // Now being in the second frame we can verify that the saved value of the
    // global JSON object indeed can be accessed.
    params.clear();
    result.clear();
    params.Set("expression",
               "window.cdc_adoQpoasnfa76pfcZLmcfl_JSON.stringify(321)");
    status = client->SendCommandAndGetResultWithTimeout(
        "Runtime.evaluate", params, &timeout, &result);
    EXPECT_TRUE(StatusOk(status));
    value = result.FindStringByDottedPath("result.value");
    EXPECT_THAT(value, testing::Pointee(testing::Eq("321")))
        << ToString(result);
  } else {
    VLOG(0) << "frame has not changed, the test has no effect";
  }
}
