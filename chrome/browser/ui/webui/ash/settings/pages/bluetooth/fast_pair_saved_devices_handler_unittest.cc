// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/bluetooth/fast_pair_saved_devices_handler.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/repository/fast_pair/mock_fast_pair_image_decoder.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash::settings {

namespace {

const char kLoadSavedDevicePage[] = "loadSavedDevicePage";
const char kRemoveSavedDevice[] = "removeSavedDevice";
const char kOptInStatusMessage[] = "fast-pair-saved-devices-opt-in-status";
const char kSavedDevicesListMessage[] = "fast-pair-saved-devices-list";

const char kSavedDeviceNameKey[] = "name";
const char kSavedDeviceImageUrlKey[] = "imageUrl";
const char kSavedDeviceAccountKeyKey[] = "accountKey";

const char kDisplayUrlBase64[] =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAIAAAD/"
    "gAIDAAAA5klEQVR4nO3QQQkAIADAQLV/"
    "Z63gXiLcJRibYw8urdcBPzErMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMC"
    "swKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCsw"
    "KzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKz"
    "ArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCswKzArMCs4iV8Bx6UARfcA"
    "AAAASUVORK5CYII=";

const char kDeviceName1[] = "I16max";
const char kImageBytes1[] = "01010101001010101010101010101";
const std::vector<uint8_t> kAccountKey1 = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                           0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                           0x61, 0xC3, 0x32, 0x1D};
const char kDeviceName2[] = "JBL Flip 6";
const char kImageBytes2[] = "111110101001010101001010101111";
const std::vector<uint8_t> kAccountKey2 = {0xA1, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                           0xF7, 0xB6, 0xCD, 0x5E, 0x3F, 0x45,
                                           0x61, 0xC3, 0x32, 0x1D};
const char kDeviceName3[] = "Pixel Buds";
const char kImageBytes3[] = "00000010101100110101010010101001";
const std::vector<uint8_t> kAccountKey3 = {0xA6, 0xB0, 0xF0, 0xBB, 0x95, 0x1F,
                                           0xF7, 0xB6, 0xCD, 0x5E, 0x3F, 0x45,
                                           0x68, 0xC3, 0x32, 0x1D};
const char kDeviceName4[] = "Wyze Buds";
const char kImageBytes4[] = "11111000101010010101";
const std::vector<uint8_t> kAccountKey4 = {0xB0, 0xB6, 0xF0, 0xBB, 0x95, 0x1F,
                                           0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                           0x61, 0xC3, 0x32, 0x1D};

const char kDeviceName5[] = "B&O Beoplay E6";
const char kImageBytes5[] = "110000100010000100010100000001001000100001";
const std::vector<uint8_t> kAccountKey5 = {0xC0, 0xC6, 0xD0, 0xBB, 0x95, 0x1F,
                                           0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                           0x61, 0xC3, 0x32, 0x1D};

const char kDeviceName6[] = "LG HBS-830";
const char kImageBytes6[] = "11110100011111111010100101001010100101010011";
const std::vector<uint8_t> kAccountKey6 = {0xB5, 0xB6, 0xF0, 0xBB, 0x95, 0x1F,
                                           0xF7, 0xB8, 0xCF, 0x5E, 0x3F, 0x45,
                                           0x61, 0xC3, 0x36, 0x1D};

const char kSavedDeviceRemoveResultMetricName[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.Remove.Result";
const char kSavedDevicesTotalUxLoadTimeMetricName[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.TotalUxLoadTime";
const char kSavedDevicesCountMetricName[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.DeviceCount";

nearby::fastpair::FastPairDevice CreateFastPairDevice(
    const std::string device_name,
    const std::string image_bytes,
    const std::vector<uint8_t>& account_key) {
  nearby::fastpair::StoredDiscoveryItem item;
  item.set_title(device_name);
  item.set_icon_png(image_bytes);

  nearby::fastpair::FastPairDevice device;
  device.set_account_key(std::string(account_key.begin(), account_key.end()));

  std::string item_str;
  item.SerializeToString(&item_str);
  device.set_discovery_item_bytes(item_str);
  return device;
}

std::string EncodeKey(const std::vector<uint8_t>& decoded_key) {
  return base::HexEncode(decoded_key);
}

void AssertDeviceInList(const base::Value& device,
                        const std::string& expected_device_name,
                        const std::string& expected_base64_image_url,
                        const std::vector<uint8_t> expected_account_key) {
  const auto* device_dict = device.GetIfDict();

  const base::Value* device_name = device_dict->Find(kSavedDeviceNameKey);
  const base::Value* device_url = device_dict->Find(kSavedDeviceImageUrlKey);
  const base::Value* device_account_key =
      device_dict->Find(kSavedDeviceAccountKeyKey);

  ASSERT_EQ(expected_device_name, device_name->GetString());
  ASSERT_EQ(expected_base64_image_url, device_url->GetString());
  ASSERT_EQ(EncodeKey(expected_account_key), device_account_key->GetString());
}

class TestFastPairSavedDevicesHandler : public FastPairSavedDevicesHandler {
 public:
  explicit TestFastPairSavedDevicesHandler(
      std::unique_ptr<quick_pair::FastPairImageDecoder> image_decoder)
      : FastPairSavedDevicesHandler(std::move(image_decoder)) {}
  ~TestFastPairSavedDevicesHandler() override = default;

  // Make public for testing.
  using FastPairSavedDevicesHandler::AllowJavascript;
  using FastPairSavedDevicesHandler::RegisterMessages;
  using FastPairSavedDevicesHandler::set_web_ui;
};

}  // namespace

class FastPairSavedDevicesHandlerTest : public testing::Test {
 public:
  FastPairSavedDevicesHandlerTest() = default;
  FastPairSavedDevicesHandlerTest(const FastPairSavedDevicesHandlerTest&) =
      delete;
  FastPairSavedDevicesHandlerTest& operator=(
      const FastPairSavedDevicesHandlerTest&) = delete;
  ~FastPairSavedDevicesHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_web_ui_ = std::make_unique<content::TestWebUI>();

    test_image_ = gfx::test::CreateImage(100, 100);
    auto mock_decoder =
        std::make_unique<quick_pair::MockFastPairImageDecoder>();
    mock_decoder_ = mock_decoder.get();
    // On call to DecodeImage, run the third argument callback with test_image_.
    ON_CALL(*mock_decoder, DecodeImage(testing::_, testing::_, testing::_))
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<2>(test_image_));

    handler_ = std::make_unique<TestFastPairSavedDevicesHandler>(
        std::move(mock_decoder));
    handler_->set_web_ui(test_web_ui_.get());
    handler_->RegisterMessages();
    fast_pair_repository_ =
        std::make_unique<quick_pair::FakeFastPairRepository>();
  }

  content::TestWebUI* test_web_ui() { return test_web_ui_.get(); }

  void InitializeSavedDevicesList(const std::string& device_name1,
                                  const std::string& device_image_bytes1,
                                  const std::vector<uint8_t>& account_key1,
                                  const std::string& device_name2,
                                  const std::string& device_image_bytes2,
                                  const std::vector<uint8_t>& account_key2,
                                  const std::string& device_name3,
                                  const std::string& device_image_bytes3,
                                  const std::vector<uint8_t>& account_key3,
                                  nearby::fastpair::OptInStatus opt_in_status) {
    std::vector<nearby::fastpair::FastPairDevice> devices{
        CreateFastPairDevice(/*device_name=*/device_name1,
                             /*image_bytes=*/device_image_bytes1,
                             /*account_key=*/account_key1),
        CreateFastPairDevice(/*device_name=*/device_name2,
                             /*image_bytes=*/device_image_bytes2,
                             /*account_key=*/account_key2),
        CreateFastPairDevice(/*device_name=*/device_name3,
                             /*image_bytes=*/device_image_bytes3,
                             /*account_key=*/account_key3)};
    fast_pair_repository_->SetSavedDevices(
        /*status=*/opt_in_status,
        /*devices=*/std::move(devices));
  }

  void VerifySavedDevicesList(
      const content::TestWebUI::CallData& saved_devices_list_call_data,
      const std::string& device_name1,
      const std::string& expected_device_url1,
      const std::vector<uint8_t>& account_key1,
      const std::string& device_name2,
      const std::string& expected_device_url2,
      const std::vector<uint8_t>& account_key2,
      const std::string& device_name3,
      const std::string& expected_device_url3,
      const std::vector<uint8_t>& account_key3) {
    // The call is structured such that the first argument is the name of the
    // first message being sent, and the second argument is the name of the
    // second message being sent.
    ASSERT_EQ(kSavedDevicesListMessage,
              saved_devices_list_call_data.arg1()->GetString());

    // Size should be in sync with size of devices list created in
    // |InitializeSavedDevicesList|
    const base::Value::List* saved_devices_list =
        saved_devices_list_call_data.arg2()->GetIfList();
    ASSERT_EQ(3u, saved_devices_list->size());

    AssertDeviceInList(/*device=*/*(saved_devices_list->begin()),
                       /*expected_device_name=*/device_name1,
                       /*expected_base64_image_url=*/expected_device_url1,
                       /*expected_account_key=*/account_key1);
    AssertDeviceInList(/*device=*/*(saved_devices_list->begin() + 1),
                       /*expected_device_name=*/device_name2,
                       /*expected_base64_image_url=*/expected_device_url2,
                       /*expected_account_key=*/account_key2);
    AssertDeviceInList(/*device=*/*(saved_devices_list->begin() + 2),
                       /*expected_device_name=*/device_name3,
                       /*expected_base64_image_url=*/expected_device_url3,
                       /*expected_account_key=*/account_key3);
  }

  void VerifyEmptySavedDevicesList(
      const content::TestWebUI::CallData& saved_devices_list_call_data) {
    ASSERT_EQ(kSavedDevicesListMessage,
              saved_devices_list_call_data.arg1()->GetString());
    const base::Value::List* saved_devices_list =
        saved_devices_list_call_data.arg2()->GetIfList();
    ASSERT_TRUE(saved_devices_list->empty());
  }

  void VerifyOptInStatus(
      const content::TestWebUI::CallData& opt_in_status_call_data,
      nearby::fastpair::OptInStatus opt_in_status) {
    // The call is structured such that the first argument is the name of the
    // first message being sent, and the second argument is the name of the
    // second message being sent.
    ASSERT_EQ(kOptInStatusMessage, opt_in_status_call_data.arg1()->GetString());
    ASSERT_EQ(static_cast<int>(opt_in_status),
              opt_in_status_call_data.arg2()->GetInt());
  }

  void LoadPage() {
    // `HandleReceivedMessages` has to use a Value::List due to the API.
    base::Value::List args;
    test_web_ui()->HandleReceivedMessage(kLoadSavedDevicePage, args);
  }

  void RemoveDevice(const std::vector<uint8_t>& account_key) {
    // `HandleReceivedMessages` has to use a Value::List due to the API.
    base::Value::List args;
    args.Append(EncodeKey(account_key));
    test_web_ui()->HandleReceivedMessage(kRemoveSavedDevice, args);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<quick_pair::FakeFastPairRepository> fast_pair_repository_;
  gfx::Image test_image_;
  raw_ptr<quick_pair::MockFastPairImageDecoder, DanglingUntriaged>
      mock_decoder_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<TestFastPairSavedDevicesHandler> handler_;
  base::WeakPtrFactory<FastPairSavedDevicesHandlerTest> weak_ptr_factory_{this};
};

TEST_F(FastPairSavedDevicesHandlerTest, GetSavedDevices) {
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      0);
  InitializeSavedDevicesList(
      /*device_name1=*/kDeviceName1, /*device_image_bytes1=*/kImageBytes1,
      /*account_key1=*/kAccountKey1, /*device_name2=*/kDeviceName2,
      /*device_image_bytes2=*/kImageBytes2, /*account_key2=*/kAccountKey2,
      /*device_name3=*/kDeviceName3, /*device_image_bytes3=*/kImageBytes3,
      /*account_key3=*/kAccountKey3,
      /*opt_in_status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  LoadPage();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[0],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_IN);

  // We mock the image decoder to return the same test image, which is why
  // the base64 encoded images are all the same here.
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[1], /*device_name1=*/kDeviceName1,
      /*expected_device_url1=*/kDisplayUrlBase64, /*account_key1=*/kAccountKey1,
      /*device_name2=*/kDeviceName2, /*expected_device_url2=*/kDisplayUrlBase64,
      /*account_key2=*/kAccountKey2, /*device_name3=*/kDeviceName3,
      /*expected_device_url3=*/kDisplayUrlBase64,
      /*account_key3=*/kAccountKey3);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      1);
}

// TODO(crbug.com/1362118): Fix flaky test.
TEST_F(FastPairSavedDevicesHandlerTest, DISABLED_ReloadBeforePageLoadsIgnored) {
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      0);
  InitializeSavedDevicesList(
      /*device_name1=*/kDeviceName1, /*device_image_bytes1=*/kImageBytes1,
      /*account_key1=*/kAccountKey1, /*device_name2=*/kDeviceName2,
      /*device_image_bytes2=*/kImageBytes2, /*account_key2=*/kAccountKey2,
      /*device_name3=*/kDeviceName3, /*device_image_bytes3=*/kImageBytes3,
      /*account_key3=*/kAccountKey3,
      /*opt_in_status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_IN);

  // Simulate two page loads at the same time.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&FastPairSavedDevicesHandlerTest::LoadPage,
                     weak_ptr_factory_.GetWeakPtr()));
  LoadPage();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // If we load the page while we are already fetching and decoding saved
  // device data, we should ignore the second load attempt and it should
  // behave like if only one call was made.
  EXPECT_EQ(2u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[0],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[1], /*device_name1=*/kDeviceName1,
      /*expected_device_url1=*/kDisplayUrlBase64, /*account_key1=*/kAccountKey1,
      /*device_name2=*/kDeviceName2, /*expected_device_url2=*/kDisplayUrlBase64,
      /*account_key2=*/kAccountKey2, /*device_name3=*/kDeviceName3,
      /*expected_device_url3=*/kDisplayUrlBase64,
      /*account_key3=*/kAccountKey3);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      1);
}

TEST_F(FastPairSavedDevicesHandlerTest, ReloadAfterPageLoads) {
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      0);
  InitializeSavedDevicesList(
      /*device_name1=*/kDeviceName1, /*device_image_bytes1=*/kImageBytes1,
      /*account_key1=*/kAccountKey1, /*device_name2=*/kDeviceName2,
      /*device_image_bytes2=*/kImageBytes2, /*account_key2=*/kAccountKey2,
      /*device_name3=*/kDeviceName3, /*device_image_bytes3=*/kImageBytes3,
      /*account_key3=*/kAccountKey3,
      /*opt_in_status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  LoadPage();
  base::RunLoop().RunUntilIdle();

  // We only expect two calls at this point: one for the opt in status, one
  // for the saved device list.
  EXPECT_EQ(2u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[0],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[1], /*device_name1=*/kDeviceName1,
      /*expected_device_url1=*/kDisplayUrlBase64, /*account_key1=*/kAccountKey1,
      /*device_name2=*/kDeviceName2, /*expected_device_url2=*/kDisplayUrlBase64,
      /*account_key2=*/kAccountKey2, /*device_name3=*/kDeviceName3,
      /*expected_device_url3=*/kDisplayUrlBase64,
      /*account_key3=*/kAccountKey3);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      1);

  LoadPage();
  base::RunLoop().RunUntilIdle();

  // We expect now the WebUI to have handled four calls: two opt in status,
  // two for saved device list.
  EXPECT_EQ(4u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[0],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[1], /*device_name1=*/kDeviceName1,
      /*expected_device_url1=*/kDisplayUrlBase64, /*account_key1=*/kAccountKey1,
      /*device_name2=*/kDeviceName2, /*expected_device_url2=*/kDisplayUrlBase64,
      /*account_key2=*/kAccountKey2, /*device_name3=*/kDeviceName3,
      /*expected_device_url3=*/kDisplayUrlBase64,
      /*account_key3=*/kAccountKey3);
  VerifyOptInStatus(*test_web_ui()->call_data()[2],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[1], /*device_name1=*/kDeviceName1,
      /*expected_device_url1=*/kDisplayUrlBase64, /*account_key1=*/kAccountKey1,
      /*device_name2=*/kDeviceName2, /*expected_device_url2=*/kDisplayUrlBase64,
      /*account_key2=*/kAccountKey2, /*device_name3=*/kDeviceName3,
      /*expected_device_url3=*/kDisplayUrlBase64,
      /*account_key3=*/kAccountKey3);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      2);
}

TEST_F(FastPairSavedDevicesHandlerTest, EmptyListSentToWebUi) {
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      0);
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/0, 0);
  fast_pair_repository_->SetSavedDevices(
      /*status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_OUT,
      /*devices=*/{});
  LoadPage();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[0],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  VerifyEmptySavedDevicesList(*test_web_ui()->call_data()[1]);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      1);
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/0, 1);
}

TEST_F(FastPairSavedDevicesHandlerTest, SavedDevicesBecomesEmpty) {
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      0);
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/0, 0);
  InitializeSavedDevicesList(
      /*device_name1=*/kDeviceName1, /*device_image_bytes1=*/kImageBytes1,
      /*account_key1=*/kAccountKey1, /*device_name2=*/kDeviceName2,
      /*device_image_bytes2=*/kImageBytes2, /*account_key2=*/kAccountKey2,
      /*device_name3=*/kDeviceName3, /*device_image_bytes3=*/kImageBytes3,
      /*account_key3=*/kAccountKey3,
      /*opt_in_status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  LoadPage();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[0],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[1], /*device_name1=*/kDeviceName1,
      /*expected_device_url1=*/kDisplayUrlBase64, /*account_key1=*/kAccountKey1,
      /*device_name2=*/kDeviceName2, /*expected_device_url2=*/kDisplayUrlBase64,
      /*account_key2=*/kAccountKey2, /*device_name3=*/kDeviceName3,
      /*expected_device_url3=*/kDisplayUrlBase64,
      /*account_key3=*/kAccountKey3);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      1);
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/0, 0);

  fast_pair_repository_->SetSavedDevices(
      /*status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_OUT,
      /*devices=*/{});
  LoadPage();
  base::RunLoop().RunUntilIdle();

  // At this point, we have had four total calls handled by our webui, but now
  // we should have the data reflect any empty list.
  EXPECT_EQ(4u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[2],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  VerifyEmptySavedDevicesList(*test_web_ui()->call_data()[3]);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      2);
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/0, 1);
}

TEST_F(FastPairSavedDevicesHandlerTest, SavedDevicesChanges) {
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      0);
  InitializeSavedDevicesList(
      /*device_name1=*/kDeviceName1, /*device_image_bytes1=*/kImageBytes1,
      /*account_key1=*/kAccountKey1, /*device_name2=*/kDeviceName2,
      /*device_image_bytes2=*/kImageBytes2, /*account_key2=*/kAccountKey2,
      /*device_name3=*/kDeviceName3, /*device_image_bytes3=*/kImageBytes3,
      /*account_key3=*/kAccountKey3,
      /*opt_in_status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  LoadPage();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[0],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[1], /*device_name1=*/kDeviceName1,
      /*expected_device_url1=*/kDisplayUrlBase64, /*account_key1=*/kAccountKey1,
      /*device_name2=*/kDeviceName2, /*expected_device_url2=*/kDisplayUrlBase64,
      /*account_key2=*/kAccountKey2, /*device_name3=*/kDeviceName3,
      /*expected_device_url3=*/kDisplayUrlBase64,
      /*account_key3=*/kAccountKey3);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      1);

  InitializeSavedDevicesList(
      /*device_name1=*/kDeviceName4, /*device_image_bytes1=*/kImageBytes4,
      /*account_key1=*/kAccountKey4, /*device_name2=*/kDeviceName5,
      /*device_image_bytes2=*/kImageBytes5, /*account_key2=*/kAccountKey5,
      /*device_name3=*/kDeviceName6, /*device_image_bytes3=*/kImageBytes6,
      /*account_key3=*/kAccountKey6,
      /*opt_in_status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  LoadPage();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(4u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[2],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[3], /*device_name1=*/kDeviceName4,
      /*expected_device_url1=*/kDisplayUrlBase64, /*account_key1=*/kAccountKey4,
      /*device_name2=*/kDeviceName5, /*expected_device_url2=*/kDisplayUrlBase64,
      /*account_key2=*/kAccountKey5, /*device_name3=*/kDeviceName6,
      /*expected_device_url3=*/kDisplayUrlBase64,
      /*account_key3=*/kAccountKey6);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      2);
}

TEST_F(FastPairSavedDevicesHandlerTest, EmptyImageSentToWebUi) {
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      0);
  ON_CALL(*mock_decoder_, DecodeImage(testing::_, testing::_, testing::_))
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<2>(gfx::Image()));
  InitializeSavedDevicesList(
      /*device_name1=*/kDeviceName1, /*device_image_bytes1=*/kImageBytes1,
      /*account_key1=*/kAccountKey1, /*device_name2=*/kDeviceName2,
      /*device_image_bytes2=*/kImageBytes2, /*account_key2=*/kAccountKey2,
      /*device_name3=*/kDeviceName3, /*device_image_bytes3=*/kImageBytes3,
      /*account_key3=*/kAccountKey3,
      /*opt_in_status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  LoadPage();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, test_web_ui()->call_data().size());
  VerifyOptInStatus(*test_web_ui()->call_data()[0],
                    nearby::fastpair::OptInStatus::STATUS_OPTED_IN);

  // If we decode an empty image, we will send a null image url to the WebUI.
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[1], /*device_name1=*/kDeviceName1,
      /*expected_device_url1=*/"", /*account_key1=*/kAccountKey1,
      /*device_name2=*/kDeviceName2, /*expected_device_url2=*/"",
      /*account_key2=*/kAccountKey2, /*device_name3=*/kDeviceName3,
      /*expected_device_url3=*/"", /*account_key3=*/kAccountKey3);
  histogram_tester().ExpectTotalCount(kSavedDevicesTotalUxLoadTimeMetricName,
                                      1);
}

TEST_F(FastPairSavedDevicesHandlerTest, RemoveSavedDevice) {
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/3, 0);
  InitializeSavedDevicesList(
      /*device_name1=*/kDeviceName1, /*device_image_bytes1=*/kImageBytes1,
      /*account_key1=*/kAccountKey1, /*device_name2=*/kDeviceName2,
      /*device_image_bytes2=*/kImageBytes2, /*account_key2=*/kAccountKey2,
      /*device_name3=*/kDeviceName3, /*device_image_bytes3=*/kImageBytes3,
      /*account_key3=*/kAccountKey3,
      /*opt_in_status=*/nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  LoadPage();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, test_web_ui()->call_data().size());
  VerifySavedDevicesList(
      *test_web_ui()->call_data()[1], /*device_name1=*/kDeviceName1,
      /*expected_device_url1=*/kDisplayUrlBase64, /*account_key1=*/kAccountKey1,
      /*device_name2=*/kDeviceName2, /*expected_device_url2=*/kDisplayUrlBase64,
      /*account_key2=*/kAccountKey2, /*device_name3=*/kDeviceName3,
      /*expected_device_url3=*/kDisplayUrlBase64,
      /*account_key3=*/kAccountKey3);
  histogram_tester().ExpectBucketCount(kSavedDeviceRemoveResultMetricName,
                                       /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(kSavedDeviceRemoveResultMetricName,
                                       /*success=*/false, 0);
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/2, 0);
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/3, 1);

  RemoveDevice(kAccountKey3);
  base::RunLoop().RunUntilIdle();

  LoadPage();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4u, test_web_ui()->call_data().size());

  const base::Value::List* saved_devices_list =
      test_web_ui()->call_data()[3]->arg2()->GetIfList();
  ASSERT_EQ(2u, saved_devices_list->size());
  AssertDeviceInList(/*device=*/*(saved_devices_list->begin()),
                     /*expected_device_name=*/kDeviceName1,
                     /*expected_base64_image_url=*/kDisplayUrlBase64,
                     /*expected_account_key=*/kAccountKey1);
  AssertDeviceInList(/*device=*/*(saved_devices_list->begin() + 1),
                     /*expected_device_name=*/kDeviceName2,
                     /*expected_base64_image_url=*/kDisplayUrlBase64,
                     /*expected_account_key=*/kAccountKey2);

  histogram_tester().ExpectBucketCount(kSavedDeviceRemoveResultMetricName,
                                       /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(kSavedDeviceRemoveResultMetricName,
                                       /*success=*/false, 0);
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/2, 1);
  histogram_tester().ExpectBucketCount(kSavedDevicesCountMetricName,
                                       /*num_devices=*/3, 1);
}

}  // namespace ash::settings
