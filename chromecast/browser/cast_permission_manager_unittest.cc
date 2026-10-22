// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_permission_manager.h"

#include "chromecast/browser/cast_permission_user_data.h"
#include "components/cast_receiver/browser/permissions_manager_impl.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace chromecast {
namespace shell {

class CastPermissionManagerTest : public content::RenderViewHostTestHarness {
 protected:
  CastPermissionManagerTest() { gl::GLSurfaceTestSupport::InitializeOneOff(); }
  ~CastPermissionManagerTest() override = default;

  void TearDown() override {
    // Ensure async ThreadPool/IO tasks spawned during navigation (such as
    // in-process NetworkService initialization) complete before
    // RenderViewHostTestHarness::TearDown() clears the
    // SystemDnsConfigChangeNotifier reference on HostResolverManager.
    task_environment()->RunUntilIdle();
    content::RenderViewHostTestHarness::TearDown();
  }

  content::TestContentClientInitializer initializer_;
  CastPermissionManager permission_manager_;
};

TEST_F(CastPermissionManagerTest, AttackerOriginDeniedWhenNotEnforced) {
  auto web_contents = CreateTestWebContents();
  const GURL app_url("https://legitimate.example");
  const GURL attacker_url("https://attacker.example");

  // Create CastPermissionUserData with enforce_feature_permissions = false
  new CastPermissionUserData(web_contents.get(), "POCAPPID", app_url,
                             /*enforce_feature_permissions=*/false,
                             /*feature_permissions=*/{},
                             /*additional_feature_permission_origins=*/{});

  // Navigate the main frame to the attacker URL.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(attacker_url);

  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();

  // Query permissions for the attacker frame.
  auto audio_descriptor = blink::mojom::PermissionDescriptor::New();
  audio_descriptor->name = blink::mojom::PermissionName::AUDIO_CAPTURE;
  content::PermissionResult audio_result =
      permission_manager_.GetPermissionResultForCurrentDocument(
          audio_descriptor, main_rfh, /*should_include_device_status=*/false);
  EXPECT_EQ(audio_result.status, blink::mojom::PermissionStatus::DENIED);

  auto video_descriptor = blink::mojom::PermissionDescriptor::New();
  video_descriptor->name = blink::mojom::PermissionName::VIDEO_CAPTURE;
  content::PermissionResult video_result =
      permission_manager_.GetPermissionResultForCurrentDocument(
          video_descriptor, main_rfh, /*should_include_device_status=*/false);
  EXPECT_EQ(video_result.status, blink::mojom::PermissionStatus::DENIED);
}

TEST_F(CastPermissionManagerTest, AppOriginGrantedWhenNotEnforced) {
  auto web_contents = CreateTestWebContents();
  const GURL app_url("https://legitimate.example");

  new CastPermissionUserData(web_contents.get(), "POCAPPID", app_url,
                             /*enforce_feature_permissions=*/false,
                             /*feature_permissions=*/{},
                             /*additional_feature_permission_origins=*/{});

  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(app_url);

  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();

  auto audio_descriptor = blink::mojom::PermissionDescriptor::New();
  audio_descriptor->name = blink::mojom::PermissionName::AUDIO_CAPTURE;
  content::PermissionResult audio_result =
      permission_manager_.GetPermissionResultForCurrentDocument(
          audio_descriptor, main_rfh, /*should_include_device_status=*/false);
  EXPECT_EQ(audio_result.status, blink::mojom::PermissionStatus::GRANTED);
}

TEST_F(CastPermissionManagerTest, AdditionalOriginsAllowedWhenNotEnforced) {
  auto web_contents = CreateTestWebContents();
  const GURL app_url("https://legitimate.example");
  const GURL allowed_url("https://allowed.example");

  new CastPermissionUserData(
      web_contents.get(), "POCAPPID", app_url,
      /*enforce_feature_permissions=*/false,
      /*feature_permissions=*/{},
      /*additional_feature_permission_origins=*/{"https://allowed.example"});

  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(allowed_url);

  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();

  auto audio_descriptor = blink::mojom::PermissionDescriptor::New();
  audio_descriptor->name = blink::mojom::PermissionName::AUDIO_CAPTURE;
  content::PermissionResult audio_result =
      permission_manager_.GetPermissionResultForCurrentDocument(
          audio_descriptor, main_rfh, /*should_include_device_status=*/false);
  EXPECT_EQ(audio_result.status, blink::mojom::PermissionStatus::GRANTED);
}

TEST_F(CastPermissionManagerTest, RespectsPermissionsManager) {
  auto web_contents = CreateTestWebContents();
  const GURL app_url("https://legitimate.example");
  const GURL attacker_url("https://attacker.example");

  // Create PermissionsManagerImpl and add allowed permissions only for app URL.
  auto* permissions_manager =
      cast_receiver::PermissionsManagerImpl::CreateInstance(*web_contents,
                                                            "POCAPPID");
  permissions_manager->AddPermission(blink::PermissionType::AUDIO_CAPTURE);
  permissions_manager->AddOrigin(url::Origin::Create(app_url));

  // Navigate to attacker.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(attacker_url);

  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();

  auto audio_descriptor = blink::mojom::PermissionDescriptor::New();
  audio_descriptor->name = blink::mojom::PermissionName::AUDIO_CAPTURE;
  content::PermissionResult audio_result =
      permission_manager_.GetPermissionResultForCurrentDocument(
          audio_descriptor, main_rfh, /*should_include_device_status=*/false);
  // Attacker should be denied by PermissionsManager.
  EXPECT_EQ(audio_result.status, blink::mojom::PermissionStatus::DENIED);

  // Navigate to app URL.
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(app_url);
  main_rfh = web_contents->GetPrimaryMainFrame();

  audio_result = permission_manager_.GetPermissionResultForCurrentDocument(
      audio_descriptor, main_rfh, /*should_include_device_status=*/false);
  // App URL should be granted by PermissionsManager.
  EXPECT_EQ(audio_result.status, blink::mojom::PermissionStatus::GRANTED);
}

}  // namespace shell
}  // namespace chromecast
