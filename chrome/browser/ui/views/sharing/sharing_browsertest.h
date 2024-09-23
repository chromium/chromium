// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_BROWSERTEST_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_BROWSERTEST_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "components/sharing_message/sharing_message_bridge.h"
#include "components/sharing_message/sharing_service.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sharing_message/web_push/web_push_sender.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "url/gurl.h"

class PageActionIconView;

class FakeWebPushSender : public WebPushSender {
 public:
  FakeWebPushSender() : WebPushSender(/*url_loader_factory=*/nullptr) {}

  FakeWebPushSender(const FakeWebPushSender&) = delete;
  FakeWebPushSender& operator=(const FakeWebPushSender&) = delete;

  ~FakeWebPushSender() override = default;

  void SendMessage(const std::string& fcm_token,
                   crypto::ECPrivateKey* vapid_key,
                   WebPushMessage message,
                   WebPushCallback callback) override;

  const std::string& fcm_token() { return fcm_token_; }
  const WebPushMessage& message() { return message_; }

 private:
  std::string fcm_token_;
  WebPushMessage message_;
};

class FakeSharingMessageBridge : public SharingMessageBridge {
 public:
  FakeSharingMessageBridge() = default;

  FakeSharingMessageBridge(const FakeSharingMessageBridge&) = delete;
  FakeSharingMessageBridge& operator=(const FakeSharingMessageBridge&) = delete;

  ~FakeSharingMessageBridge() override = default;

  // SharingMessageBridge:
  void SendSharingMessage(
      std::unique_ptr<sync_pb::SharingMessageSpecifics> specifics,
      CommitFinishedCallback on_commit_callback) override;

  // SharingMessageBridge:
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

  const sync_pb::SharingMessageSpecifics& specifics() const {
    return specifics_;
  }

 private:
  sync_pb::SharingMessageSpecifics specifics_;
};

// Base test class for testing sharing features.
class SharingBrowserTest : public SyncTest {
 public:
  SharingBrowserTest();

  SharingBrowserTest(const SharingBrowserTest&) = delete;
  SharingBrowserTest& operator=(const SharingBrowserTest&) = delete;

  ~SharingBrowserTest() override;

  void SetUpOnMainThread() override;

  void Init(
      sync_pb::SharingSpecificFields_EnabledFeatures first_device_feature,
      sync_pb::SharingSpecificFields_EnabledFeatures second_device_feature);

  virtual std::string GetTestPageURL() const = 0;

  std::unique_ptr<TestRenderViewContextMenu> InitContextMenu(
      const GURL& url,
      std::string_view link_text,
      std::string_view selection_text);

  void CheckLastReceiver(const SharingTargetDeviceInfo& device) const;

  components_sharing_message::SharingMessage GetLastSharingMessageSent() const;

  SharingService* sharing_service() const;

  content::WebContents* web_contents() const;

  PageActionIconView* GetPageActionIconView(PageActionIconType type);

 private:
  void SetUpDevices(
      sync_pb::SharingSpecificFields_EnabledFeatures first_device_feature,
      sync_pb::SharingSpecificFields_EnabledFeatures second_device_feature);

  void RegisterDevice(int profile_index,
                      sync_pb::SharingSpecificFields_EnabledFeatures feature);
  void AddDeviceInfo(const syncer::DeviceInfo& original_device,
                     int fake_device_id);

  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  syncer::FakeDeviceInfoTracker fake_device_info_tracker_;
  std::vector<std::unique_ptr<syncer::DeviceInfo>> device_infos_;
  raw_ptr<SharingService, AcrossTasksDanglingUntriaged> sharing_service_;
  raw_ptr<FakeWebPushSender, AcrossTasksDanglingUntriaged>
      fake_web_push_sender_;
  FakeSharingMessageBridge fake_sharing_message_bridge_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_BROWSERTEST_H_
