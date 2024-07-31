// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sync/test/integration/sessions_helper.h"
#include "chrome/browser/ui/views/sharing/sharing_browsertest.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/sync/service/sync_service_impl.h"
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

  SharedClipboardBrowserTestBase(const SharedClipboardBrowserTestBase&) =
      delete;
  SharedClipboardBrowserTestBase& operator=(
      const SharedClipboardBrowserTestBase&) = delete;

  ~SharedClipboardBrowserTestBase() override = default;

  std::string GetTestPageURL() const override {
    return std::string(kTestPageURL);
  }

  void CheckLastSharingMessageSent(const std::string& expected_text) const {
    components_sharing_message::SharingMessage sharing_message =
        GetLastSharingMessageSent();
    ASSERT_TRUE(sharing_message.has_shared_clipboard_message());
    ASSERT_EQ(expected_text, sharing_message.shared_clipboard_message().text());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

using SharedClipboardUIFeatureDisabledBrowserTest =
    SharedClipboardBrowserTestBase;

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
