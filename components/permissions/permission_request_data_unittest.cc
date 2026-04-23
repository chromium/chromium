// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_request_data.h"

#include <memory>
#include <vector>

#include "components/permissions/contexts/geolocation_permission_context.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "url/gurl.h"

namespace permissions {

namespace {

class TestDelegate : public GeolocationPermissionContext::Delegate {
 public:
  bool DecidePermission(const PermissionRequestData& request_data,
                        BrowserPermissionCallback* callback,
                        GeolocationPermissionContext* context) override {
    return false;
  }
#if BUILDFLAG(IS_ANDROID)
  bool IsInteractable(content::WebContents* web_contents) override {
    return true;
  }
  PrefService* GetPrefs(content::BrowserContext* browser_context) override {
    return nullptr;
  }
  bool IsRequestingOriginDSE(content::BrowserContext* browser_context,
                             const GURL& requesting_origin) override {
    return false;
  }
#endif
};

}  // namespace

class PermissionRequestDataTest : public testing::Test {
 protected:
  PermissionRequestDataTest() = default;
  ~PermissionRequestDataTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  TestPermissionsClient client_;
  content::TestBrowserContext browser_context_;
};

TEST_F(PermissionRequestDataTest, GeolocationApproximateAccuracy) {
  GeolocationPermissionContext geolocation_context(
      &browser_context_, std::make_unique<TestDelegate>());

  PermissionRequestID id(content::GlobalRenderFrameHostId(1, 1),
                         PermissionRequestID::RequestLocalId(1));
  GURL origin("https://example.com");

  blink::mojom::PermissionDescriptorPtr descriptor =
      blink::mojom::PermissionDescriptor::New(
          blink::mojom::PermissionName::GEOLOCATION_APPROXIMATE, nullptr);
  std::vector<blink::mojom::PermissionDescriptorPtr> permissions;
  permissions.push_back(descriptor.Clone());

  content::PermissionRequestDescription request_description(
      std::move(permissions), /*user_gesture=*/true, origin);

  PermissionRequestData request_data(id, request_description, origin);

  EXPECT_EQ(GeolocationAccuracy::kApproximate,
            request_data.GetRequestedGeolocationAccuracy());
}

TEST_F(PermissionRequestDataTest, GeolocationPreciseAccuracy) {
  GeolocationPermissionContext geolocation_context(
      &browser_context_, std::make_unique<TestDelegate>());

  PermissionRequestID id(content::GlobalRenderFrameHostId(1, 1),
                         PermissionRequestID::RequestLocalId(1));
  GURL origin("https://example.com");

  blink::mojom::PermissionDescriptorPtr descriptor =
      blink::mojom::PermissionDescriptor::New(
          blink::mojom::PermissionName::GEOLOCATION, nullptr);
  std::vector<blink::mojom::PermissionDescriptorPtr> permissions;
  permissions.push_back(std::move(descriptor));

  content::PermissionRequestDescription request_description(
      std::move(permissions), /*user_gesture=*/true, origin);

  PermissionRequestData request_data(id, request_description, origin);

  EXPECT_EQ(GeolocationAccuracy::kPrecise,
            request_data.GetRequestedGeolocationAccuracy());
}

}  // namespace permissions
