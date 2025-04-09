// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/serial_chooser_dialog_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/android/device_dialog/serial_chooser_dialog_android.h"
#include "chrome/browser/ui/serial/mock_serial_chooser_controller.h"
#include "chrome/browser/ui/serial/serial_chooser_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace {

using testing::_;
using testing::Return;

const std::u16string kDevice1 = u"Test Device";
const std::u16string kDevice2 = u"Test Pixel Buds";
const std::u16string kDevice3 = u"ttyS0";

class MockJavaDialog : public SerialChooserDialogAndroid::JavaDialog {
 public:
  MockJavaDialog();

  MOCK_METHOD(void, Close, ());
  MOCK_METHOD(void, SetIdleState, (), (const));
  MOCK_METHOD(void,
              AddDevice,
              (const std::string&, const std::u16string&),
              (const));
  MOCK_METHOD(void, RemoveDevice, (const std::string&), (const));
  MOCK_METHOD(void, OnAdapterEnabledChanged, (bool), (const));
  MOCK_METHOD(void, OnAdapterAuthorizationChanged, (bool), (const));
};

MockJavaDialog::MockJavaDialog()
    : SerialChooserDialogAndroid::JavaDialog(nullptr) {}

class SerialChooserDialogAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  SerialChooserDialogAndroidTest();
  ~SerialChooserDialogAndroidTest() override;

  void SetUp() override;

 protected:
  void CreateDialog();
  void OnDialogClosed();

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;

  base::MockCallback<SerialChooserDialogAndroid::CreateJavaDialogCallback>
      mock_callback_;
  raw_ptr<testing::NiceMock<MockSerialChooserController>> mock_controller_;
  raw_ptr<MockJavaDialog> mock_java_dialog_;

  std::unique_ptr<SerialChooserDialogAndroid> dialog_;
};

SerialChooserDialogAndroidTest::SerialChooserDialogAndroidTest() = default;
SerialChooserDialogAndroidTest::~SerialChooserDialogAndroidTest() = default;

void SerialChooserDialogAndroidTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  NavigateAndCommit(GURL("https://main-frame.com"));
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(main_rfh());
  window_ = ui::WindowAndroid::CreateForTesting();
  window_.get()->get()->AddChild(web_contents->GetNativeView());
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents);
}

void SerialChooserDialogAndroidTest::CreateDialog() {
  std::unique_ptr<MockJavaDialog> mock_java_dialog =
      std::make_unique<MockJavaDialog>();
  mock_java_dialog_ = mock_java_dialog.get();
  EXPECT_CALL(mock_callback_, Run(/*env=*/_, /*window_android=*/_,
                                  /*origin=*/_,
                                  /*security_level=*/_, /*profile=*/_,
                                  /*native_serial_chooser_dialog_ptr=*/_))
      .WillOnce(Return(std::move(mock_java_dialog)));

  std::unique_ptr<testing::NiceMock<MockSerialChooserController>>
      mock_controller =
          std::make_unique<testing::NiceMock<MockSerialChooserController>>(u"");

  mock_controller_ = mock_controller.get();
  ON_CALL(*mock_controller_, NumOptions()).WillByDefault(Return(2));
  ON_CALL(*mock_controller_, GetOption(0)).WillByDefault(Return(kDevice1));
  ON_CALL(*mock_controller_, GetOption(1)).WillByDefault(Return(kDevice2));
  ON_CALL(*mock_controller_, GetOption(2)).WillByDefault(Return(kDevice3));

  dialog_ = SerialChooserDialogAndroid::CreateForTesting(
      main_rfh(), std::move(mock_controller),
      base::BindOnce(&SerialChooserDialogAndroidTest::OnDialogClosed,
                     base::Unretained(this)),
      mock_callback_.Get());
}

void SerialChooserDialogAndroidTest::OnDialogClosed() {
  dialog_.reset();
  mock_controller_ = nullptr;
  mock_java_dialog_ = nullptr;
}

TEST_F(SerialChooserDialogAndroidTest, FrameTree) {
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub-frame.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids),
      base::BindLambdaForTesting(
          [](device::mojom::SerialPortInfoPtr serial_port_info) {}));

  auto origin_predicate = [&](const std::u16string& java_string) {
    return java_string == u"https://main-frame.com";
  };
  EXPECT_CALL(mock_callback_, Run(/*env=*/_, /*window_android=*/_,
                                  testing::Truly(origin_predicate),
                                  /*security_level=*/_, /*profile=*/_,
                                  /*native_serial_chooser_dialog_ptr=*/_));
  SerialChooserDialogAndroid::CreateForTesting(
      subframe, std::move(controller), base::BindLambdaForTesting([]() {}),
      mock_callback_.Get());
}

TEST_F(SerialChooserDialogAndroidTest, OnOptionAdded) {
  CreateDialog();

  EXPECT_CALL(*mock_java_dialog_, AddDevice("0", kDevice1));
  dialog_->OnOptionAdded(0);
}

TEST_F(SerialChooserDialogAndroidTest, OnOptionInitialized) {
  CreateDialog();

  EXPECT_CALL(*mock_java_dialog_, AddDevice("0", kDevice1));
  EXPECT_CALL(*mock_java_dialog_, AddDevice("1", kDevice2));
  EXPECT_CALL(*mock_java_dialog_, SetIdleState());
  dialog_->OnOptionsInitialized();
}

TEST_F(SerialChooserDialogAndroidTest, OnOptionAddedAndInitialized) {
  CreateDialog();

  dialog_->OnOptionAdded(0);
  dialog_->OnOptionAdded(1);
  dialog_->OnOptionAdded(2);

  EXPECT_CALL(*mock_java_dialog_, RemoveDevice("0"));
  EXPECT_CALL(*mock_java_dialog_, RemoveDevice("1"));
  EXPECT_CALL(*mock_java_dialog_, RemoveDevice("2"));
  EXPECT_CALL(*mock_java_dialog_, AddDevice("3", kDevice1));
  EXPECT_CALL(*mock_java_dialog_, AddDevice("4", kDevice2));
  EXPECT_CALL(*mock_java_dialog_, SetIdleState());
  dialog_->OnOptionsInitialized();
}

TEST_F(SerialChooserDialogAndroidTest, OnOptionRemoved) {
  CreateDialog();

  dialog_->OnOptionsInitialized();

  EXPECT_CALL(*mock_java_dialog_, RemoveDevice("0"));
  dialog_->OnOptionRemoved(0);
}

TEST_F(SerialChooserDialogAndroidTest, ListDevices) {
  CreateDialog();

  EXPECT_CALL(*mock_controller_, RefreshOptions());
  dialog_->ListDevices(base::android::AttachCurrentThread());
}

TEST_F(SerialChooserDialogAndroidTest, OnItemSelected) {
  CreateDialog();

  dialog_->OnOptionsInitialized();

  std::vector<size_t> selected_items = {1u};
  EXPECT_CALL(*mock_controller_, Select(selected_items));
  std::string item_id = "1";
  dialog_->OnItemSelected(base::android::AttachCurrentThread(), item_id);
}

TEST_F(SerialChooserDialogAndroidTest, OnDialogCancelled) {
  CreateDialog();

  EXPECT_CALL(*mock_controller_, Cancel());

  dialog_->OnDialogCancelled(base::android::AttachCurrentThread());

  EXPECT_FALSE(dialog_);
  EXPECT_FALSE(mock_controller_);
  EXPECT_FALSE(mock_java_dialog_);
}

TEST_F(SerialChooserDialogAndroidTest, OnAdapterEnabledChanged) {
  CreateDialog();

  EXPECT_CALL(*mock_java_dialog_, OnAdapterEnabledChanged(false));
  dialog_->OnAdapterEnabledChanged(false);
}

TEST_F(SerialChooserDialogAndroidTest, OnAdapterAuthorizationChanged) {
  CreateDialog();

  EXPECT_CALL(*mock_java_dialog_, OnAdapterAuthorizationChanged(false));
  dialog_->OnAdapterAuthorizationChanged(false);
}

TEST_F(SerialChooserDialogAndroidTest, OpenAdapterOffHelpPage) {
  CreateDialog();

  EXPECT_CALL(*mock_controller_, OpenAdapterOffHelpUrl());
  EXPECT_CALL(*mock_controller_, Cancel());

  dialog_->OpenAdapterOffHelpPage(base::android::AttachCurrentThread());

  EXPECT_FALSE(dialog_);
  EXPECT_FALSE(mock_controller_);
  EXPECT_FALSE(mock_java_dialog_);
}

TEST_F(SerialChooserDialogAndroidTest, OpenBluetoothPermissionHelpPage) {
  CreateDialog();

  EXPECT_CALL(*mock_controller_, OpenBluetoothPermissionHelpUrl());
  EXPECT_CALL(*mock_controller_, Cancel());

  dialog_->OpenBluetoothPermissionHelpPage(
      base::android::AttachCurrentThread());

  EXPECT_FALSE(dialog_);
  EXPECT_FALSE(mock_controller_);
  EXPECT_FALSE(mock_java_dialog_);
}

TEST_F(SerialChooserDialogAndroidTest, OpenSerialHelpPage) {
  CreateDialog();

  EXPECT_CALL(*mock_controller_, OpenHelpCenterUrl());
  EXPECT_CALL(*mock_controller_, Cancel());

  dialog_->OpenSerialHelpPage(base::android::AttachCurrentThread());

  EXPECT_FALSE(dialog_);
  EXPECT_FALSE(mock_controller_);
  EXPECT_FALSE(mock_java_dialog_);
}

}  // namespace
