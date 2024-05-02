// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/midi_permission_context.h"

#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

using PermissionStatus = blink::mojom::PermissionStatus;

class MidiPermissionContextTests : public testing::Test {
 public:
  content::TestBrowserContext* browser_context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestPermissionsClient client_;
};

// Web MIDI permission status should be allowed only for secure origins.
TEST_F(MidiPermissionContextTests, TestNoSysexAllowedAllOrigins) {
  MidiPermissionContext permission_context(browser_context());
  GURL insecure_url("http://www.example.com");
  GURL secure_url("https://www.example.com");

  EXPECT_EQ(PermissionStatus::DENIED,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, insecure_url)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, secure_url)
                .status);

  EXPECT_EQ(PermissionStatus::ASK,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     secure_url, secure_url)
                .status);
}

}  // namespace permissions
