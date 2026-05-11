// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/tabs_from_other_devices/synced_screenshot_data_source.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/mock_session_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

using testing::_;
using testing::IsNull;
using testing::Return;

class SyncedScreenshotDataSourceTest : public ChromeRenderViewHostTestHarness {
 public:
  SyncedScreenshotDataSourceTest() = default;

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            SessionSyncServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<sync_sessions::MockSessionSyncService>();
            })},
    };
  }

  sync_sessions::MockSessionSyncService* mock_session_sync_service() {
    return static_cast<sync_sessions::MockSessionSyncService*>(
        SessionSyncServiceFactory::GetForProfile(profile()));
  }

  void StartDataRequest(const GURL& url,
                        content::URLDataSource::GotDataCallback callback) {
    SyncedScreenshotDataSource data_source;
    data_source.StartDataRequest(
        url,
        base::BindRepeating([](content::WebContents* wc) { return wc; },
                            web_contents()),
        std::move(callback));
  }
};

TEST_F(SyncedScreenshotDataSourceTest, GetSource) {
  SyncedScreenshotDataSource data_source;
  EXPECT_EQ(data_source.GetSource(), "synced-screenshot");
}

TEST_F(SyncedScreenshotDataSourceTest, GetMimeType) {
  SyncedScreenshotDataSource data_source;
  EXPECT_EQ(data_source.GetMimeType(GURL()), "image/jpg");
}

TEST_F(SyncedScreenshotDataSourceTest, StartDataRequest_ValidURL) {
  const std::string session_tag = "session_123";
  const SessionID tab_id = SessionID::FromSerializedValue(456);
  const std::string screenshot_data = "fake_jpeg_data";

  EXPECT_CALL(*mock_session_sync_service(),
              ReadTabScreenshot(session_tag, tab_id, _))
      .WillOnce(
          [&](const std::string&, SessionID,
              base::OnceCallback<void(std::optional<std::string>)> callback) {
            std::move(callback).Run(screenshot_data);
          });

  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&](scoped_refptr<base::RefCountedMemory> data) {
        ASSERT_TRUE(data);
        std::string returned_data(reinterpret_cast<const char*>(data->front()),
                                  data->size());
        EXPECT_EQ(returned_data, screenshot_data);
      });

  const GURL url(absl::StrFormat("chrome://synced-screenshot/%s/%d",
                                 session_tag, tab_id.id()));
  StartDataRequest(url, callback.Get());
}

TEST_F(SyncedScreenshotDataSourceTest, StartDataRequest_ValidURL_WithQuery) {
  const std::string session_tag = "session_123";
  const SessionID tab_id = SessionID::FromSerializedValue(456);
  const std::string screenshot_data = "fake_jpeg_data";

  EXPECT_CALL(*mock_session_sync_service(),
              ReadTabScreenshot(session_tag, tab_id, _))
      .WillOnce(
          [&](const std::string&, SessionID,
              base::OnceCallback<void(std::optional<std::string>)> callback) {
            std::move(callback).Run(screenshot_data);
          });

  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&](scoped_refptr<base::RefCountedMemory> data) {
        ASSERT_TRUE(data);
        std::string returned_data(reinterpret_cast<const char*>(data->front()),
                                  data->size());
        EXPECT_EQ(returned_data, screenshot_data);
      });

  const GURL url(
      absl::StrFormat("chrome://synced-screenshot/%s/%d?timestamp=789",
                      session_tag, tab_id.id()));
  StartDataRequest(url, callback.Get());
}

TEST_F(SyncedScreenshotDataSourceTest, StartDataRequest_InvalidURL_NoSlash) {
  EXPECT_CALL(*mock_session_sync_service(), ReadTabScreenshot(_, _, _))
      .Times(0);

  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(IsNull()));

  const GURL url("chrome://synced-screenshot/invalid_no_slash");
  StartDataRequest(url, callback.Get());
}

TEST_F(SyncedScreenshotDataSourceTest, StartDataRequest_InvalidURL_InvalidId) {
  EXPECT_CALL(*mock_session_sync_service(), ReadTabScreenshot(_, _, _))
      .Times(0);

  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(IsNull()));

  const GURL url("chrome://synced-screenshot/session_123/abc");
  StartDataRequest(url, callback.Get());
}

TEST_F(SyncedScreenshotDataSourceTest, StartDataRequest_NotFound) {
  const std::string session_tag = "session_123";
  const SessionID tab_id = SessionID::FromSerializedValue(456);

  EXPECT_CALL(*mock_session_sync_service(),
              ReadTabScreenshot(session_tag, tab_id, _))
      .WillOnce(
          [&](const std::string&, SessionID,
              base::OnceCallback<void(std::optional<std::string>)> callback) {
            std::move(callback).Run(std::nullopt);
          });

  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(IsNull()));

  const GURL url(absl::StrFormat("chrome://synced-screenshot/%s/%d",
                                 session_tag, tab_id.id()));
  StartDataRequest(url, callback.Get());
}
