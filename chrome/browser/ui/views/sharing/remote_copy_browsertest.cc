// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/shared_clipboard/remote_copy_handle_message_result.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {
const char kDeviceName[] = "test device name";
const char kText[] = "test text";
const char kResultHistogram[] = "Sharing.RemoteCopyHandleMessageResult";
const char kTextSizeHistogram[] = "Sharing.RemoteCopyReceivedTextSize";
const char kImageSizeBeforeDecodeHistogram[] =
    "Sharing.RemoteCopyReceivedImageSizeBeforeDecode";
const char kImageSizeAfterDecodeHistogram[] =
    "Sharing.RemoteCopyReceivedImageSizeAfterDecode";
const char kStatusCodeHistogram[] = "Sharing.RemoteCopyLoadImageStatusCode";
const char kLoadTimeHistogram[] = "Sharing.RemoteCopyLoadImageTime";
const char kDecodeTimeHistogram[] = "Sharing.RemoteCopyDecodeImageTime";
const char kResizeTimeHistogram[] = "Sharing.RemoteCopyResizeImageTime";

class ClipboardObserver : public ui::ClipboardObserver {
 public:
  explicit ClipboardObserver(base::RepeatingClosure callback)
      : callback_(callback) {}

  void OnClipboardDataChanged() override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardObserver);
};

}  // namespace

// Browser tests for the Remote Copy feature.
class RemoteCopyBrowserTestBase : public InProcessBrowserTest {
 public:
  RemoteCopyBrowserTestBase() = default;
  ~RemoteCopyBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    ui::TestClipboard::CreateForCurrentThread();
    notification_tester_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
    sharing_service_ =
        SharingServiceFactory::GetForBrowserContext(browser()->profile());
  }

  void TearDownOnMainThread() override {
    notification_tester_.reset();
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  gcm::IncomingMessage CreateMessage(const std::string& device_name,
                                     base::Optional<std::string> text,
                                     base::Optional<GURL> image_url) {
    chrome_browser_sharing::SharingMessage sharing_message;
    sharing_message.set_sender_guid(base::GenerateGUID());
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
        CreateMessage(device_name, text, /*image_url=*/base::nullopt));
  }

  void SendImageMessage(const std::string& device_name, const GURL& image_url) {
    base::RunLoop run_loop;
    ClipboardObserver observer(run_loop.QuitClosure());
    ui::ClipboardMonitor::GetInstance()->AddObserver(&observer);
    sharing_service_->GetFCMHandlerForTesting()->OnMessage(
        kSharingFCMAppID,
        CreateMessage(device_name, /*text*/ base::nullopt, image_url));
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
    return ui::clipboard_test_util::ReadImage(
        ui::Clipboard::GetForCurrentThread());
  }

  message_center::Notification GetNotification() {
    auto notifications = notification_tester_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::SHARING);
    EXPECT_EQ(notifications.size(), 1u);
    return notifications[0];
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histograms_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  SharingService* sharing_service_;
  std::unique_ptr<net::EmbeddedTestServer> server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteCopyBrowserTestBase);
};

class RemoteCopyDisabledBrowserTest : public RemoteCopyBrowserTestBase {
 public:
  RemoteCopyDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(kRemoteCopyReceiver);
  }
};

IN_PROC_BROWSER_TEST_F(RemoteCopyDisabledBrowserTest, FeatureDisabled) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());

  // Send a message with text.
  SendTextMessage(kDeviceName, kText);

  // The clipboard is still empty because the feature is disabled and the
  // handler is not installed.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());
  histograms_.ExpectTotalCount(kResultHistogram, 0);
}

class RemoteCopyBrowserTest : public RemoteCopyBrowserTestBase {
 public:
  RemoteCopyBrowserTest() {
    server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    EXPECT_TRUE(server_->Start());

    url::Origin allowlist_origin = url::Origin::Create(server_->base_url());
    feature_list_.InitWithFeaturesAndParameters(
        {{kRemoteCopyReceiver,
          {{kRemoteCopyAllowedOrigins.name, allowlist_origin.Serialize()}}},
         {kRemoteCopyImageNotification, {}}},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(RemoteCopyBrowserTest, Text) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());

  // Send a message with text.
  SendTextMessage(kDeviceName, kText);

  // The text is in the clipboard and a notification is shown.
  std::vector<std::u16string> types = GetAvailableClipboardTypes();
  size_t expected_size = 1u;
#if defined(OS_LINUX)
  // Ozone/X11 and Wayland also set kMimeTypeTextUtf8 along with kMimeTypeText.
  // TODO(https://crbug.com/1096425): remove this if condition.
  if (features::IsUsingOzonePlatform())
    expected_size = 2u;
#endif
  ASSERT_EQ(expected_size, types.size());
  ASSERT_EQ(ui::kMimeTypeText, base::UTF16ToASCII(types[0]));
  if (expected_size == 2u)
    ASSERT_EQ(ui::kMimeTypeTextUtf8, base::UTF16ToASCII(types[1]));
  ASSERT_EQ(kText, ReadClipboardText());
  message_center::Notification notification = GetNotification();
  ASSERT_EQ(l10n_util::GetStringFUTF16(
                IDS_SHARING_REMOTE_COPY_NOTIFICATION_TITLE_TEXT_CONTENT,
                base::ASCIIToUTF16(kDeviceName)),
            notification.title());
  ASSERT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  histograms_.ExpectUniqueSample(
      kResultHistogram, RemoteCopyHandleMessageResult::kSuccessHandledText, 1);
  histograms_.ExpectUniqueSample(kTextSizeHistogram, std::string(kText).size(),
                                 1);
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
                base::ASCIIToUTF16(kDeviceName)),
            notification.title());
  ASSERT_EQ(message_center::NOTIFICATION_TYPE_IMAGE, notification.type());
#if defined(OS_MAC)
  // We show the image in the notification icon on macOS.
  ASSERT_EQ(640, notification.icon().Width());
  ASSERT_EQ(480, notification.icon().Height());
#else
  ASSERT_EQ(640, notification.rich_notification_data().image.Width());
  ASSERT_EQ(480, notification.rich_notification_data().image.Height());
#endif  // defined(OS_MAC)
  histograms_.ExpectUniqueSample(kStatusCodeHistogram, net::HTTP_OK, 1);
  histograms_.ExpectTotalCount(kLoadTimeHistogram, 1);
  histograms_.ExpectUniqueSample(kImageSizeBeforeDecodeHistogram, 810490, 1);
  histograms_.ExpectTotalCount(kDecodeTimeHistogram, 1);
  histograms_.ExpectUniqueSample(kImageSizeAfterDecodeHistogram, 19660800, 1);
  histograms_.ExpectTotalCount(kResizeTimeHistogram, 1);
  histograms_.ExpectUniqueSample(
      kResultHistogram, RemoteCopyHandleMessageResult::kSuccessHandledImage, 1);
}

IN_PROC_BROWSER_TEST_F(RemoteCopyBrowserTest, TextThenImageUrl) {
  // The clipboard is empty.
  ASSERT_TRUE(GetAvailableClipboardTypes().empty());
  histograms_.ExpectTotalCount(kResultHistogram, 0);

  // Send a message with text.
  SendTextMessage(kDeviceName, kText);

  // The text is in the clipboard.
  std::vector<std::u16string> types = GetAvailableClipboardTypes();
  size_t expected_size = 1u;
#if defined(OS_LINUX)
  // Ozone/X11 and Wayland also set kMimeTypeTextUtf8 along with kMimeTypeText.
  // TODO(https://crbug.com/1096425): remove this if condition.
  if (features::IsUsingOzonePlatform())
    expected_size = 2u;
#endif
  ASSERT_EQ(expected_size, types.size());
  ASSERT_EQ(ui::kMimeTypeText, base::UTF16ToASCII(types[0]));
  if (expected_size == 2u)
    ASSERT_EQ(ui::kMimeTypeTextUtf8, base::UTF16ToASCII(types[1]));
  ASSERT_EQ(kText, ReadClipboardText());
  histograms_.ExpectTotalCount(kResultHistogram, 1);
  histograms_.ExpectUniqueSample(
      kResultHistogram, RemoteCopyHandleMessageResult::kSuccessHandledText, 1);

  // Send a message with an image url.
  SendImageMessage(kDeviceName, server_->GetURL("/image_decoding/droids.jpg"));

  // The image is in the clipboard and the text has been cleared.
  types = GetAvailableClipboardTypes();
  ASSERT_EQ(1u, types.size());
  ASSERT_EQ(ui::kMimeTypePNG, base::UTF16ToASCII(types[0]));
  ASSERT_EQ(std::string(), ReadClipboardText());
  histograms_.ExpectTotalCount(kResultHistogram, 2);
  histograms_.ExpectBucketCount(
      kResultHistogram, RemoteCopyHandleMessageResult::kSuccessHandledImage, 1);
}
