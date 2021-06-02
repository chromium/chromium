// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/ui/views/sharing/sharing_browsertest.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace {
const char kSelectedText[] = "Lorem ipsum";
const char kTestPageURL[] = "/sharing/tel.html";
}  // namespace

// Browser tests for the Shared Clipboard feature.
class SharedClipboardBrowserTestBase : public SharingBrowserTest {
 public:
  SharedClipboardBrowserTestBase() {}

  ~SharedClipboardBrowserTestBase() override = default;

  std::string GetTestPageURL() const override {
    return std::string(kTestPageURL);
  }

  void CheckLastSharingMessageSent(const std::string& expected_text) const {
    chrome_browser_sharing::SharingMessage sharing_message =
        GetLastSharingMessageSent();
    ASSERT_TRUE(sharing_message.has_shared_clipboard_message());
    ASSERT_EQ(expected_text, sharing_message.shared_clipboard_message().text());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedClipboardBrowserTestBase);
};

class SharedClipboardBrowserTest : public SharedClipboardBrowserTestBase {
 public:
  SharedClipboardBrowserTest() {
    feature_list_.InitAndEnableFeature(kSharedClipboardUI);
  }
};

IN_PROC_BROWSER_TEST_F(SharedClipboardBrowserTest, ContextMenu_SingleDevice) {
  Init(sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2);
  ASSERT_EQ(1u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(), "", kSelectedText);
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE));
  ASSERT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES));

  menu->ExecuteCommand(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE, 0);
  CheckLastReceiver(*devices[0]);
  CheckLastSharingMessageSent(kSelectedText);
}

IN_PROC_BROWSER_TEST_F(SharedClipboardBrowserTest,
                       ContextMenu_MultipleDevices) {
  Init(sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2,
       sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2);
  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(), "", kSelectedText);
  ASSERT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE));
  ASSERT_TRUE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES));

  ui::MenuModel* sub_menu_model = nullptr;
  int device_id = -1;
  ASSERT_TRUE(menu->GetMenuModelAndItemIndex(kSubMenuFirstDeviceCommandId,
                                             &sub_menu_model, &device_id));
  EXPECT_EQ(2, sub_menu_model->GetItemCount());
  EXPECT_EQ(0, device_id);

  for (auto& device : devices) {
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + device_id,
              sub_menu_model->GetCommandIdAt(device_id));
    sub_menu_model->ActivatedAt(device_id);

    CheckLastReceiver(*device);
    CheckLastSharingMessageSent(kSelectedText);
    device_id++;
  }
}

IN_PROC_BROWSER_TEST_F(SharedClipboardBrowserTest, ContextMenu_NoDevices) {
  Init(sync_pb::SharingSpecificFields::UNKNOWN,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2);
  ASSERT_EQ(0u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(), "", kSelectedText);
  ASSERT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE));
  ASSERT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES));
}

IN_PROC_BROWSER_TEST_F(SharedClipboardBrowserTest, ContextMenu_SyncTurnedOff) {
  if (base::FeatureList::IsEnabled(kSharingSendViaSync)) {
    // Turning off sync will have no effect when Shared Clipboard is available
    // on sign-in.
    return;
  }

  Init(sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2,
       sync_pb::SharingSpecificFields::UNKNOWN);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2);
  ASSERT_EQ(1u, devices.size());

  // Disable syncing preferences which is necessary for Sharing.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(false, {});
  ASSERT_TRUE(AwaitQuiescence());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(), "", kSelectedText);
  ASSERT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE));
  ASSERT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES));
}

class SharedClipboardUIFeatureDisabledBrowserTest
    : public SharedClipboardBrowserTestBase {
 public:
  SharedClipboardUIFeatureDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(kSharedClipboardUI);
  }
};

IN_PROC_BROWSER_TEST_F(SharedClipboardUIFeatureDisabledBrowserTest,
                       ContextMenu_UIFeatureDisabled) {
  Init(sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2,
       sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2);
  auto devices = sharing_service()->GetDeviceCandidates(
      sync_pb::SharingSpecificFields::SHARED_CLIPBOARD_V2);
  ASSERT_EQ(2u, devices.size());

  std::unique_ptr<TestRenderViewContextMenu> menu =
      InitContextMenu(GURL(), "", kSelectedText);
  ASSERT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE));
  ASSERT_FALSE(menu->IsItemPresent(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES));
}
