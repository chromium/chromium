// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_permission_helper.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view.mojom.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_constants.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace guest_view {

namespace {

struct PermissionEventInfo {
  int request_id = -1;
  std::string permission = "";

  bool operator==(const PermissionEventInfo& other) const = default;
};

template <typename Sink>
void AbslStringify(Sink& sink, const PermissionEventInfo& info) {
  absl::Format(&sink, "{%d, %s}", info.request_id, info.permission);
}

class TestGuestViewManagerDelegate : public GuestViewManagerDelegate {
 public:
  TestGuestViewManagerDelegate() = default;
  ~TestGuestViewManagerDelegate() override = default;

  void DispatchEvent(const std::string& event_name,
                     base::DictValue args,
                     GuestViewBase* guest,
                     int instance_id) override {
    if (event_name == guest_view::slim_web_view::kEventPermission) {
      auto request_id = args.FindInt("requestId");
      EXPECT_TRUE(request_id.has_value());
      auto* permission = args.FindString("permission");
      EXPECT_TRUE(permission != nullptr);
      permission_events_.push_back(PermissionEventInfo{
          .request_id = *request_id,
          .permission = *permission,
      });
    }
  }

  bool IsGuestAvailableToContext(const GuestViewBase* guest) const override {
    return true;
  }

  void RegisterAdditionalGuestViewTypes(GuestViewManager* manager) override {
    manager->RegisterGuestViewType(
        guest_view::SlimWebViewGuest::Type,
        base::BindRepeating(&guest_view::SlimWebViewGuest::Create),
        base::NullCallback());
  }

  const std::vector<PermissionEventInfo>& permission_events() const {
    return permission_events_;
  }

 private:
  std::vector<PermissionEventInfo> permission_events_;
};

class TestPermissionControllerDelegate : public content::MockPermissionManager {
 public:
  TestPermissionControllerDelegate() = default;
  ~TestPermissionControllerDelegate() override = default;

  void RequestPermissionsFromCurrentDocument(
      content::RenderFrameHost* render_frame_host,
      const content::PermissionRequestDescription& request_description,
      base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
          callback) override {
    std::vector<content::PermissionResult> results;
    for (size_t i = 0; i < request_description.permissions.size(); ++i) {
      results.emplace_back(blink::mojom::PermissionStatus::GRANTED,
                           content::PermissionStatusSource::UNSPECIFIED);
    }
    std::move(callback).Run(results);
  }
};

}  // namespace

class SlimWebViewPermissionHelperTest
    : public content::RenderViewHostTestHarness {
 public:
  SlimWebViewPermissionHelperTest() = default;
  ~SlimWebViewPermissionHelperTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
      GTEST_SKIP() << "SlimWebViewGuest is not supported in MPArch.";
    }
    EXPECT_TRUE(browser_context());
    content::TestBrowserContext::FromBrowserContext(browser_context())
        ->SetPermissionControllerDelegate(
            std::make_unique<TestPermissionControllerDelegate>());

    auto delegate = std::make_unique<TestGuestViewManagerDelegate>();
    delegate_ = delegate.get();
    auto* guest_view_manager =
        guest_view_manager_factory_.GetOrCreateTestGuestViewManager(
            browser_context(), std::move(delegate));

    // Navigate to any URL to set a site instance, which is required to create
    // a guest.
    NavigateAndCommit(GURL("chrome://some-webui-page"));
    guest_view_manager->CreateGuest(
        SlimWebViewGuest::Type, main_rfh(), base::DictValue(),
        base::BindLambdaForTesting(
            [&](GuestViewBase* guest) { guest_ = guest; }));

    AttachGuest();
  }

 protected:
  void AttachGuest() {
    EXPECT_TRUE(guest_);
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    content::RenderFrameHost* outer_contents_frame =
        rfh_tester->AppendChild("outer_contents_frame");

    auto owned_guest = guest_->GetGuestViewManager()->TransferOwnership(guest_);
    EXPECT_TRUE(owned_guest);
    EXPECT_TRUE(outer_contents_frame);

    guest_->AttachToOuterWebContentsFrame(
        std::move(owned_guest), outer_contents_frame,
        /*element_instance_id=*/1, /*is_full_page_plugin=*/false,
        base::NullCallback());
  }

  TestGuestViewManagerDelegate* delegate() { return delegate_; }
  SlimWebViewPermissionHelper& permission_helper() {
    return static_cast<SlimWebViewGuest*>(guest_.get())->permission_helper();
  }

 private:
  raw_ptr<TestGuestViewManagerDelegate> delegate_;
  guest_view::TestGuestViewManagerFactory guest_view_manager_factory_;
  raw_ptr<GuestViewBase> guest_ = nullptr;
};

TEST_F(SlimWebViewPermissionHelperTest, RequestGeolocationPermission) {
  base::test::TestFuture<content::PermissionResult> permission_future;

  permission_helper().RequestGeolocationPermission(
      GURL("https://example.com"),
      /*user_gesture=*/true, permission_future.GetCallback());

  std::vector<PermissionEventInfo> expected_events{
      {.request_id = 1, .permission = "geolocation"}};
  EXPECT_EQ(delegate()->permission_events(), expected_events);

  // Allow the permission.
  auto set_result = permission_helper().SetPermission(
      1, mojom::PageHandler_PermissionResponseAction::kAllow);

  EXPECT_EQ(set_result,
            SlimWebViewPermissionHelper::SetPermissionResult::kAllowed);
  EXPECT_TRUE(permission_future.Wait());
  const content::PermissionResult& final_result = permission_future.Get();
  EXPECT_EQ(final_result.status, blink::mojom::PermissionStatus::GRANTED);
}

TEST_F(SlimWebViewPermissionHelperTest, RequestDownloadPermission) {
  base::test::TestFuture<bool> permission_future;

  permission_helper().CanDownload(GURL("https://example.com"), "GET",
                                  permission_future.GetCallback());

  std::vector<PermissionEventInfo> expected_events{
      {.request_id = 1, .permission = "download"}};
  EXPECT_EQ(delegate()->permission_events(), expected_events);

  // Allow the permission.
  auto set_result = permission_helper().SetPermission(
      1, mojom::PageHandler_PermissionResponseAction::kAllow);

  EXPECT_EQ(set_result,
            SlimWebViewPermissionHelper::SetPermissionResult::kAllowed);
  EXPECT_TRUE(permission_future.Get());
}

}  // namespace guest_view
