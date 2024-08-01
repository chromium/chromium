// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/sharing_message/shared_clipboard/remote_copy_handle_message_result.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_fcm_handler.h"
#include "components/sharing_message/sharing_service.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {
const char kDeviceName[] = "test device name";
const char16_t kDeviceName16[] = u"test device name";
const char kText[] = "test text";

class ClipboardObserver : public ui::ClipboardObserver {
 public:
  explicit ClipboardObserver(base::RepeatingClosure callback)
      : callback_(callback) {}

  ClipboardObserver(const ClipboardObserver&) = delete;
  ClipboardObserver& operator=(const ClipboardObserver&) = delete;

  void OnClipboardDataChanged() override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

// Browser tests for the Remote Copy feature.
class RemoteCopyBrowserTest : public InProcessBrowserTest {
 public:
  RemoteCopyBrowserTest() {
    server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    EXPECT_TRUE(server_->Start());
  }

  ~RemoteCopyBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ui::TestClipboard::CreateForCurrentThread();
    notification_tester_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
    sharing_service_ =
        SharingServiceFactory::GetForBrowserContext(browser()->profile());
    auto* remote_copy_handler = static_cast<RemoteCopyMessageHandler*>(
        sharing_service_->GetSharingHandlerForTesting(
            components_sharing_message::SharingMessage::kRemoteCopyMessage));
    ASSERT_TRUE(remote_copy_handler);
    remote_copy_handler->set_allowed_origin_for_testing(server_->base_url());
  }

  void TearDownOnMainThread() override {
    notification_tester_.reset();
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  gcm::IncomingMessage CreateMessage(const std::string& device_name,
                                     std::optional<std::string> text,
                                     std::optional<GURL> image_url) {
    components_sharing_message::SharingMessage sharing_message;
    sharing_message.set_sender_guid(
        base::Uuid::GenerateRandomV4().AsLowercaseString());
    sharing_message.set_sender_device_name(device_name);
    if (text) {
      sharing_message.mutable_remote_copy_message()->set_text(text.value());
    } else if (image_url) {
      sharing_message.mutable_remote_copy_message()->set_image_url(
          image_url.value().possibly_invalid_spec());
    }

    gcm::IncomingMessage incoming_message;
    std::string serialized_sharing_message;
    sharing_message.SerializeToString(&serialized_sharing_message);
    incoming_message.raw_data = serialized_sharing_message;
    return incoming_message;
  }

  void SendTextMessage(const std::string& device_name,
                       const std::string& text) {
    sharing_service_->GetFCMHandlerForTesting()->OnMessage(
        kSharingFCMAppID,
        CreateMessage(device_name, text, /*image_url=*/std::nullopt));
  }

  void SendImageMessage(const std::string& device_name, const GURL& image_url) {
    base::RunLoop run_loop;
    ClipboardObserver observer(run_loop.QuitClosure());
    ui::ClipboardMonitor::GetInstance()->AddObserver(&observer);
    sharing_service_->GetFCMHandlerForTesting()->OnMessage(
        kSharingFCMAppID,
        CreateMessage(device_name, /*text*/ std::nullopt, image_url));
    run_loop.Run();
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(&observer);
  }

  std::vector<std::u16string> GetAvailableClipboardTypes() {
    std::vector<std::u16string> types;
    ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
        ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &types);
    return types;
  }

  std::string ReadClipboardText() {
    std::u16string text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &text);
    return base::UTF16ToUTF8(text);
  }

  SkBitmap ReadClipboardImage() {
    SkBitmap bitmap;
    std::vector<uint8_t> png_data =
        ui::clipboard_test_util::ReadPng(ui::Clipboard::GetForCurrentThread());
    gfx::PNGCodec::Decode(png_data.data(), png_data.size(), &bitmap);
    return bitmap;
  }

  message_center::Notification GetNotification() {
    auto notifications = notification_tester_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::SHARING);
    EXPECT_EQ(notifications.size(), 1u);
    return notifications[0];
  }

 protected:
  base::HistogramTester histograms_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  raw_ptr<SharingService, DanglingUntriaged> sharing_service_;
  std::unique_ptr<net::EmbeddedTestServer> server_;
};

IN_PROC_BROWSER_TEST_F(RemoteCopyBrowserTest, Text) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());

  // Send a message with text.
  SendTextMessage(kDeviceName, kText);

  // The text is in the clipboard and a notification is shown.
  std::vector<std::u16string> types = GetAvailableClipboardTypes();
  size_t expected_size = 1u;
  ASSERT_EQ(expected_size, types.size());
  ASSERT_EQ(ui::kMimeTypeText, base::UTF16ToASCII(types[0]));
  if (expected_size == 2u)
    ASSERT_EQ(ui::kMimeTypeTextUtf8, base::UTF16ToASCII(types[1]));
  ASSERT_EQ(kText, ReadClipboardText());
  message_center::Notification notification = GetNotification();
  ASSERT_EQ(l10n_util::GetStringFUTF16(
                IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT,
                kDeviceName16),
            notification.title());
  ASSERT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
}

IN_PROC_BROWSER_TEST_F(RemoteCopyBrowserTest, ImageUrl) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());

  // Send a message with an image url.
  SendImageMessage(kDeviceName, server_->GetURL("/image_decoding/droids.jpg"));

  // The image is in the clipboard and a notification is shown.
  std::vector<std::u16string> types = GetAvailableClipboardTypes();
  ASSERT_EQ(1u, types.size());
  ASSERT_EQ(ui::kMimeTypePNG, base::UTF16ToASCII(types[0]));
  SkBitmap bitmap = ReadClipboardImage();
  ASSERT_FALSE(bitmap.drawsNothing());
  ASSERT_EQ(2560, bitmap.width());
  ASSERT_EQ(1920, bitmap.height());
  message_center::Notification notification = GetNotification();
  ASSERT_EQ(l10n_util::GetStringFUTF16(
                IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_IMAGE_CONTENT,
                kDeviceName16),
            notification.title());
  ASSERT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
}

IN_PROC_BROWSER_TEST_F(RemoteCopyBrowserTest, TextThenImageUrl) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());

  // Send a message with text.
  SendTextMessage(kDeviceName, kText);

  // The text is in the clipboard.
  std::vector<std::u16string> types = GetAvailableClipboardTypes();
  size_t expected_size = 1u;
  ASSERT_EQ(expected_size, types.size());
  ASSERT_EQ(ui::kMimeTypeText, base::UTF16ToASCII(types[0]));
  if (expected_size == 2u)
    ASSERT_EQ(ui::kMimeTypeTextUtf8, base::UTF16ToASCII(types[1]));
  ASSERT_EQ(kText, ReadClipboardText());

  // Send a message with an image url.
  SendImageMessage(kDeviceName, server_->GetURL("/image_decoding/droids.jpg"));

  // The image is in the clipboard and the text has been cleared.
  types = GetAvailableClipboardTypes();
  ASSERT_EQ(1u, types.size());
  ASSERT_EQ(ui::kMimeTypePNG, base::UTF16ToASCII(types[0]));
  ASSERT_EQ(std::string(), ReadClipboardText());
}
