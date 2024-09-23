// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "printing/pdf_render_settings.h"
#include "printing/print_job_constants.h"
#include "printing/pwg_raster_settings.h"
#include "printing/units.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using device::mojom::UsbDeviceInfoPtr;
using extensions::Extension;
using extensions::PrinterProviderAPI;
using extensions::PrinterProviderPrintJob;
using extensions::TestExtensionEnvironment;

namespace printing {

namespace {

// Printer id used for requests in tests.
const char kPrinterId[] = "printer_id";

// Printer list used a result for getPrinters.
const char kPrinterDescriptionList[] =
    "[{"
    "  \"id\": \"printer1\","
    "  \"name\": \"Printer 1\""
    "}, {"
    "  \"id\": \"printer2\","
    "  \"name\": \"Printer 2\","
    "  \"description\": \"Test printer 2\""
    "}]";

// Printer capability for a printer that supportd only PWG raster.
const char kPWGRasterOnlyPrinterSimpleDescription[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"printer\": {"
    "    \"supported_content_type\": ["
    "      {\"content_type\": \"image/pwg-raster\"}"
    "    ]"
    "  }"
    "}";

// Print ticket with no parameters set.
const char kEmptyPrintTicket[] = "{\"version\": \"1.0\"}";

// Print ticket that has duplex parameter set.
const char kPrintTicketWithDuplex[] =
    "{"
    "  \"version\": \"1.0\","
    "  \"print\": {"
    "    \"duplex\": {\"type\": \"LONG_EDGE\"}"
    "  }"
    "}";

// An extension with permission for 1 printer it supports.
const char kExtension1[] =
    "{"
    "  \"name\": \"Provider 1\","
    "  \"app\": {"
    "    \"background\": {"
    "      \"scripts\": [\"background.js\"]"
    "    }"
    "  },"
    "  \"permissions\": ["
    "    \"printerProvider\","
    "    \"usb\","
    "    {"
    "     \"usbDevices\": ["
    "       { \"vendorId\": 0, \"productId\": 1 }"
    "     ]"
    "    },"
    "  ],"
    "  \"usb_printers\": {"
    "    \"filters\": ["
    "      { \"vendorId\": 0, \"productId\": 0 },"
    "      { \"vendorId\": 0, \"productId\": 1 }"
    "    ]"
    "  }"
    "}";

// An extension with permission for none of the printers it supports.
const char kExtension2[] =
    "{"
    "  \"name\": \"Provider 2\","
    "  \"app\": {"
    "    \"background\": {"
    "      \"scripts\": [\"background.js\"]"
    "    }"
    "  },"
    "  \"permissions\": [ \"printerProvider\", \"usb\" ],"
    "  \"usb_printers\": {"
    "    \"filters\": ["
    "      { \"vendorId\": 0, \"productId\": 0 },"
    "      { \"vendorId\": 0, \"productId\": 1 }"
    "    ]"
    "  }"
    "}";

const char kPdfSettings[] = R"({
  "deviceName": "printer_id",
  "capabilities": "{
      \"version\": \"1.0\",
      \"printer\": {
        \"supported_content_type\": [
          {\"content_type\": \"application/pdf\"},
          {\"content_type\": \"image/pwg-raster\"}
        ]
      }
    }",
  "ticket": "{\"version\": \"1.0\"}",
  "pageWidth": 100,
  "pageHeight": 50
})";

const char kAllTypesSettings[] = R"({
  "deviceName": "printer_id",
  "capabilities": "{
      \"version\": \"1.0\",
      \"printer\": {
        \"supported_content_type\": [
          {\"content_type\": \"*/*\"}
        ]
      }
    }",
  "ticket": "{\"version\": \"1.0\"}",
  "pageWidth": 100,
  "pageHeight": 50
})";

const char kSimpleRasterSettings[] = R"({
  "deviceName": "printer_id",
  "capabilities": "{
      \"version\": \"1.0\",
      \"printer\": {
        \"supported_content_type\": [
          {\"content_type\": \"image/pwg-raster\"}
        ]
      }
    }",
  "ticket": "{\"version\": \"1.0\"}",
  "pageWidth": 100,
  "pageHeight": 50
})";

const char kInvalidSettings[] = R"({
  "deviceName": "printer_id",
  "capabilities": "{
      \"version\": \"1.0\",
      \"printer\": {
        \"supported_content_type\": [
          {\"content_type\": \"image/pwg-raster\"}
        ]
      }
    }",
  "ticket": "{}",
  "pageWidth": 100,
  "pageHeight": 50
})";

const char kDuplexSettings[] = R"({
  "deviceName": "printer_id",
  "capabilities": "{
      \"version\": \"1.0\",
      \"printer\": {
        \"supported_content_type\": [
          {\"content_type\": \"image/pwg-raster\"}
        ],
        \"pwg_raster_config\": {
          \"document_sheet_back\": \"FLIPPED\",
          \"reverse_order_streaming\": true,
          \"rotate_all_pages\": true
        },
        \"dpi\": {
          \"option\": [{
            \"horizontal_dpi\": 100,
            \"vertical_dpi\": 200,
            \"is_default\": true
          }]
        }
      }
    }",
  "ticket": "{
      \"version\": \"1.0\",
      \"print\": {
        \"duplex\": {\"type\": \"LONG_EDGE\"}
      }
    }",
  "pageWidth": 100,
  "pageHeight": 50
})";

const char kContentTypePDF[] = "application/pdf";
const char kContentTypePWG[] = "image/pwg-raster";

// Print request status considered to be successful by fake PrinterProviderAPI.
const char kPrintRequestSuccess[] = "OK";

constexpr unsigned char kPrintData[] = "print data, PDF";

// Used as a callback to StartGetPrinters() in tests.
// Increases `call_count` and records values returned by StartGetPrinters().
void RecordPrinterList(size_t& call_count,
                       base::Value::List& printers_out,
                       base::Value::List printers) {
  ++call_count;
  printers_out = std::move(printers);
}

// Used as a callback to StartGetPrinters in tests.
// Records that the test is done.
void RecordPrintersDone(bool* is_done_out) {
  *is_done_out = true;
}

// Used as a callback to StartGetCapability() in tests.
// Increases `call_count` and records values returned by StartGetCapability().
void RecordCapability(size_t& call_count,
                      base::Value::Dict& capability_out,
                      base::Value::Dict capability) {
  ++call_count;
  base::Value::Dict* capabilities = capability.FindDict(kSettingCapabilities);
  if (capabilities)
    capability_out = std::move(*capabilities);
  else
    capability_out.clear();
}

// Used as a callback to StartPrint in tests.
// Increases |*call_count| and records values returned by StartPrint.
void RecordPrintResult(size_t* call_count,
                       bool* success_out,
                       std::string* status_out,
                       const base::Value& status) {
  ++(*call_count);
  bool success = status.is_none();
  std::string status_str = success ? kPrintRequestSuccess : status.GetString();
  *success_out = success;
  *status_out = status_str;
}

// Used as a callback to StartGrantPrinterAccess in tests.
// Increases |*call_count| and records the value returned.
void RecordPrinterInfo(size_t* call_count,
                       base::Value::Dict* printer_info_out,
                       const base::Value::Dict& printer_info) {
  ++(*call_count);
  *printer_info_out = printer_info.Clone();
}

std::string RefCountedMemoryToString(
    scoped_refptr<base::RefCountedMemory> memory) {
  return std::string(base::as_string_view(*memory));
}

// Fake PwgRasterConverter used in the tests.
class FakePwgRasterConverter : public PwgRasterConverter {
 public:
  FakePwgRasterConverter() {}

  FakePwgRasterConverter(const FakePwgRasterConverter&) = delete;
  FakePwgRasterConverter& operator=(const FakePwgRasterConverter&) = delete;

  ~FakePwgRasterConverter() override = default;

  // PwgRasterConverter implementation. It writes |data| to shared memory.
  // Also, remembers conversion and bitmap settings passed into the method.
  void Start(const std::optional<bool>& use_skia,
             const base::RefCountedMemory* data,
             const PdfRenderSettings& conversion_settings,
             const PwgRasterSettings& bitmap_settings,
             ResultCallback callback) override {
    base::ReadOnlySharedMemoryRegion invalid_pwg_region;
    if (fail_conversion_) {
      std::move(callback).Run(std::move(invalid_pwg_region));
      return;
    }

    base::MappedReadOnlyRegion memory =
        base::ReadOnlySharedMemoryRegion::Create(data->size());
    if (!memory.IsValid()) {
      ADD_FAILURE() << "Failed to create pwg raster shared memory.";
      std::move(callback).Run(std::move(invalid_pwg_region));
      return;
    }

    memory.mapping.GetMemoryAsSpan<uint8_t>().copy_from(*data);
    conversion_settings_ = conversion_settings;
    bitmap_settings_ = bitmap_settings;
    std::move(callback).Run(std::move(memory.region));
  }

  // Makes |Start| method always return an error.
  void FailConversion() { fail_conversion_ = true; }

  const PdfRenderSettings& conversion_settings() const {
    return conversion_settings_;
  }

  const PwgRasterSettings& bitmap_settings() const { return bitmap_settings_; }

 private:
  PdfRenderSettings conversion_settings_;
  PwgRasterSettings bitmap_settings_;
  bool fail_conversion_ = false;
};

// Information about received print requests.
struct PrintRequestInfo {
  PrinterProviderAPI::PrintCallback callback;
  PrinterProviderPrintJob job;
};

// Fake PrinterProviderAPI used in tests.
// It caches requests issued to API and exposes methods to trigger their
// callbacks.
class FakePrinterProviderAPI : public PrinterProviderAPI {
 public:
  FakePrinterProviderAPI() = default;

  FakePrinterProviderAPI(const FakePrinterProviderAPI&) = delete;
  FakePrinterProviderAPI& operator=(const FakePrinterProviderAPI&) = delete;

  ~FakePrinterProviderAPI() override = default;

  void DispatchGetPrintersRequested(
      const PrinterProviderAPI::GetPrintersCallback& callback) override {
    pending_printers_callbacks_.push(callback);
  }

  void DispatchGetCapabilityRequested(
      const std::string& destination_id,
      PrinterProviderAPI::GetCapabilityCallback callback) override {
    pending_capability_callbacks_.push(std::move(callback));
  }

  void DispatchPrintRequested(
      PrinterProviderPrintJob job,
      PrinterProviderAPI::PrintCallback callback) override {
    PrintRequestInfo request_info;
    request_info.callback = std::move(callback);
    request_info.job = std::move(job);

    pending_print_requests_.push(std::move(request_info));
  }

  void DispatchGetUsbPrinterInfoRequested(
      const std::string& extension_id,
      const device::mojom::UsbDeviceInfo& device,
      PrinterProviderAPI::GetPrinterInfoCallback callback) override {
    EXPECT_EQ("fake extension id", extension_id);
    EXPECT_FALSE(device.guid.empty());
    pending_usb_info_callbacks_.push(std::move(callback));
  }

  size_t pending_get_printers_count() const {
    return pending_printers_callbacks_.size();
  }

  const PrinterProviderPrintJob* GetPrintJob(
      const extensions::Extension* extension,
      int request_id) const override {
    ADD_FAILURE() << "Not reached";
    return nullptr;
  }

  void TriggerNextGetPrintersCallback(base::Value::List printers, bool done) {
    ASSERT_GT(pending_get_printers_count(), 0u);
    pending_printers_callbacks_.front().Run(std::move(printers), done);
    pending_printers_callbacks_.pop();
  }

  size_t pending_get_capability_count() const {
    return pending_capability_callbacks_.size();
  }

  void TriggerNextGetCapabilityCallback(base::Value::Dict caps) {
    ASSERT_GT(pending_get_capability_count(), 0u);
    std::move(pending_capability_callbacks_.front()).Run(std::move(caps));
    pending_capability_callbacks_.pop();
  }

  size_t pending_print_count() const { return pending_print_requests_.size(); }

  const PrinterProviderPrintJob* GetNextPendingPrintJob() const {
    EXPECT_GT(pending_print_count(), 0u);
    if (pending_print_count() == 0)
      return nullptr;
    return &pending_print_requests_.front().job;
  }

  void TriggerNextPrintCallback(const std::string& result) {
    ASSERT_GT(pending_print_count(), 0u);
    base::Value result_value;
    if (result != kPrintRequestSuccess)
      result_value = base::Value(result);
    std::move(pending_print_requests_.front().callback).Run(result_value);
    pending_print_requests_.pop();
  }

  size_t pending_usb_info_count() const {
    return pending_usb_info_callbacks_.size();
  }

  void TriggerNextUsbPrinterInfoCallback(base::Value::Dict printer_info) {
    ASSERT_GT(pending_usb_info_count(), 0u);
    std::move(pending_usb_info_callbacks_.front()).Run(std::move(printer_info));
    pending_usb_info_callbacks_.pop();
  }

 private:
  base::queue<PrinterProviderAPI::GetPrintersCallback>
      pending_printers_callbacks_;
  base::queue<PrinterProviderAPI::GetCapabilityCallback>
      pending_capability_callbacks_;
  base::queue<PrintRequestInfo> pending_print_requests_;
  base::queue<PrinterProviderAPI::GetPrinterInfoCallback>
      pending_usb_info_callbacks_;
};

std::unique_ptr<KeyedService> BuildTestingPrinterProviderAPI(
    content::BrowserContext* context) {
  return std::make_unique<FakePrinterProviderAPI>();
}

}  // namespace

class ExtensionPrinterHandlerTest : public testing::Test {
 public:
  ExtensionPrinterHandlerTest() = default;

  ExtensionPrinterHandlerTest(const ExtensionPrinterHandlerTest&) = delete;
  ExtensionPrinterHandlerTest& operator=(const ExtensionPrinterHandlerTest&) =
      delete;

  ~ExtensionPrinterHandlerTest() override = default;

  void SetUp() override {
    extensions::PrinterProviderAPIFactory::GetInstance()->SetTestingFactory(
        env_.profile(), base::BindRepeating(&BuildTestingPrinterProviderAPI));
    extension_printer_handler_ =
        std::make_unique<ExtensionPrinterHandler>(env_.profile());

    auto pwg_raster_converter = std::make_unique<FakePwgRasterConverter>();
    pwg_raster_converter_ = pwg_raster_converter.get();
    extension_printer_handler_->SetPwgRasterConverterForTesting(
        std::move(pwg_raster_converter));

    // Set fake USB device manager for extensions::UsbDeviceManager.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
    fake_usb_manager_.AddReceiver(usb_manager.InitWithNewPipeAndPassReceiver());
    extensions::UsbDeviceManager::Get(env_.profile())
        ->SetDeviceManagerForTesting(std::move(usb_manager));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  FakePrinterProviderAPI* GetPrinterProviderAPI() {
    return static_cast<FakePrinterProviderAPI*>(
        extensions::PrinterProviderAPIFactory::GetInstance()
            ->GetForBrowserContext(env_.profile()));
  }

  device::FakeUsbDeviceManager fake_usb_manager_;
  TestExtensionEnvironment env_;
  std::unique_ptr<ExtensionPrinterHandler> extension_printer_handler_;

  // Owned by |extension_printer_handler_|.
  raw_ptr<FakePwgRasterConverter> pwg_raster_converter_ = nullptr;
};

TEST_F(ExtensionPrinterHandlerTest, GetPrinters) {
  size_t call_count = 0;
  base::Value::List printers;
  bool is_done = false;
  extension_printer_handler_->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, &is_done));

  EXPECT_EQ(0u, call_count);
  EXPECT_TRUE(printers.empty());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_printers_count());

  base::Value original_printers =
      base::test::ParseJson(kPrinterDescriptionList);
  ASSERT_TRUE(original_printers.is_list());

  fake_api->TriggerNextGetPrintersCallback(original_printers.GetList().Clone(),
                                           /*done=*/true);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(is_done);
  EXPECT_EQ(printers, original_printers.GetList());
}

TEST_F(ExtensionPrinterHandlerTest, GetPrintersReset) {
  size_t call_count = 0;
  base::Value::List printers;
  bool is_done = false;
  extension_printer_handler_->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, &is_done));

  EXPECT_EQ(0u, call_count);
  EXPECT_TRUE(printers.empty());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_printers_count());

  extension_printer_handler_->Reset();

  base::Value original_printers =
      base::test::ParseJson(kPrinterDescriptionList);
  ASSERT_TRUE(original_printers.is_list());

  fake_api->TriggerNextGetPrintersCallback(original_printers.GetList().Clone(),
                                           /*done=*/true);

  EXPECT_EQ(0u, call_count);
}

TEST_F(ExtensionPrinterHandlerTest, GetUsbPrinters) {
  UsbDeviceInfoPtr device0 =
      fake_usb_manager_.CreateAndAddDevice(0, 0, "Google", "USB Printer", "");
  UsbDeviceInfoPtr device1 =
      fake_usb_manager_.CreateAndAddDevice(0, 1, "Google", "USB Printer", "");
  base::RunLoop().RunUntilIdle();

  const Extension* extension_1 =
      env_.MakeExtension(base::test::ParseJsonDict(kExtension1),
                         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const Extension* extension_2 =
      env_.MakeExtension(base::test::ParseJsonDict(kExtension2),
                         "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

  extensions::DevicePermissionsManager* permissions_manager =
      extensions::DevicePermissionsManager::Get(env_.profile());
  permissions_manager->AllowUsbDevice(extension_2->id(), *device0);

  size_t call_count = 0;
  base::Value::List printers;
  bool is_done = false;
  extension_printer_handler_->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, &is_done));

  base::RunLoop().RunUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_printers_count());

  EXPECT_EQ(1u, call_count);
  EXPECT_FALSE(is_done);
  EXPECT_EQ(2u, printers.size());
  base::Value::Dict extension_1_entry =
      base::Value::Dict()
          .Set("id", base::StringPrintf("provisional-usb:%s:%s",
                                        extension_1->id().c_str(),
                                        device0->guid.c_str()))
          .Set("name", "USB Printer")
          .Set("extensionName", "Provider 1")
          .Set("extensionId", extension_1->id())
          .Set("provisional", true);
  base::Value::Dict extension_2_entry =
      base::Value::Dict()
          .Set("id", base::StringPrintf("provisional-usb:%s:%s",
                                        extension_2->id().c_str(),
                                        device1->guid.c_str()))
          .Set("name", "USB Printer")
          .Set("extensionName", "Provider 2")
          .Set("extensionId", extension_2->id())
          .Set("provisional", true);
  EXPECT_TRUE(base::Contains(printers, extension_1_entry));
  EXPECT_TRUE(base::Contains(printers, extension_2_entry));

  fake_api->TriggerNextGetPrintersCallback(base::Value::List(), /*done=*/true);

  EXPECT_EQ(1u, call_count);  // No printers, so no calls. Call count stays 1.
  EXPECT_TRUE(is_done);       // Still calls done.
  EXPECT_EQ(2u, printers.size());
}

TEST_F(ExtensionPrinterHandlerTest, GetCapability) {
  size_t call_count = 0;
  base::Value::Dict capability;

  extension_printer_handler_->StartGetCapability(
      kPrinterId, base::BindOnce(&RecordCapability, std::ref(call_count),
                                 std::ref(capability)));

  EXPECT_EQ(0u, call_count);

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_capability_count());

  base::Value original_capability =
      base::test::ParseJson(kPWGRasterOnlyPrinterSimpleDescription);
  ASSERT_TRUE(original_capability.is_dict());

  // TODO(thestig): Consolidate constants used in this section.
  base::Value::Dict original_capability_with_dpi_dict =
      original_capability.GetDict().Clone();
  base::Value::Dict* printer =
      original_capability_with_dpi_dict.FindDict("printer");
  ASSERT_TRUE(printer);
  auto default_dpi_option =
      base::Value::Dict().Set("horizontal_dpi", kDefaultPdfDpi);
  default_dpi_option.Set("vertical_dpi", kDefaultPdfDpi);
  auto dpi_list = base::Value::List().Append(std::move(default_dpi_option));
  base::Value::Dict dpi_dict;
  dpi_dict.Set("option", std::move(dpi_list));
  printer->Set("dpi", std::move(dpi_dict));

  fake_api->TriggerNextGetCapabilityCallback(
      std::move(original_capability).TakeDict());

  EXPECT_EQ(1u, call_count);
  EXPECT_EQ(capability, original_capability_with_dpi_dict);
}

TEST_F(ExtensionPrinterHandlerTest, GetCapabilityReset) {
  size_t call_count = 0;
  base::Value::Dict capability;

  extension_printer_handler_->StartGetCapability(
      kPrinterId, base::BindOnce(&RecordCapability, std::ref(call_count),
                                 std::ref(capability)));

  EXPECT_EQ(0u, call_count);

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_get_capability_count());

  extension_printer_handler_->Reset();

  base::Value original_capability =
      base::test::ParseJson(kPWGRasterOnlyPrinterSimpleDescription);
  ASSERT_TRUE(original_capability.is_dict());

  fake_api->TriggerNextGetCapabilityCallback(
      original_capability.GetDict().Clone());

  EXPECT_EQ(0u, call_count);
}

TEST_F(ExtensionPrinterHandlerTest, PrintPdf) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data =
      base::MakeRefCounted<base::RefCountedStaticMemory>(kPrintData);
  std::u16string title = u"Title";

  extension_printer_handler_->StartPrint(
      title, std::move(base::test::ParseJson(kPdfSettings).GetDict()),
      print_data,
      base::BindOnce(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  const PrinterProviderPrintJob* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(title, print_job->job_title);
  EXPECT_EQ(base::test::ParseJson(kEmptyPrintTicket), print_job->ticket);
  EXPECT_EQ(kContentTypePDF, print_job->content_type);
  ASSERT_TRUE(print_job->document_bytes);
  EXPECT_EQ(RefCountedMemoryToString(print_data),
            RefCountedMemoryToString(print_job->document_bytes));

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, PrintPdfReset) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data = base::MakeRefCounted<base::RefCountedBytes>(kPrintData);
  std::u16string title = u"Title";

  extension_printer_handler_->StartPrint(
      title, std::move(base::test::ParseJson(kPdfSettings).GetDict()),
      print_data,
      base::BindOnce(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  extension_printer_handler_->Reset();

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(0u, call_count);
}

TEST_F(ExtensionPrinterHandlerTest, PrintAll) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data = base::MakeRefCounted<base::RefCountedBytes>(kPrintData);
  std::u16string title = u"Title";

  extension_printer_handler_->StartPrint(
      title, std::move(base::test::ParseJson(kAllTypesSettings).GetDict()),
      print_data,
      base::BindOnce(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  const PrinterProviderPrintJob* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(title, print_job->job_title);
  EXPECT_EQ(base::test::ParseJson(kEmptyPrintTicket), print_job->ticket);
  EXPECT_EQ(kContentTypePDF, print_job->content_type);
  ASSERT_TRUE(print_job->document_bytes);
  EXPECT_EQ(RefCountedMemoryToString(print_data),
            RefCountedMemoryToString(print_job->document_bytes));

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, PrintPwg) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data = base::MakeRefCounted<base::RefCountedBytes>(kPrintData);
  std::u16string title = u"Title";

  extension_printer_handler_->StartPrint(
      title, std::move(base::test::ParseJson(kSimpleRasterSettings).GetDict()),
      print_data,
      base::BindOnce(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  content::RunAllTasksUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  EXPECT_EQ(mojom::DuplexMode::kSimplex,
            pwg_raster_converter_->bitmap_settings().duplex_mode);
  EXPECT_EQ(TRANSFORM_NORMAL,
            pwg_raster_converter_->bitmap_settings().odd_page_transform);
  EXPECT_FALSE(pwg_raster_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_FALSE(pwg_raster_converter_->bitmap_settings().reverse_page_order);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().use_color);

  EXPECT_EQ(gfx::Size(kDefaultPdfDpi, kDefaultPdfDpi),
            pwg_raster_converter_->conversion_settings().dpi);
  EXPECT_TRUE(pwg_raster_converter_->conversion_settings().autorotate);
  // size = vertically_oriented_size * vertical_dpi / points_per_inch x
  //        horizontally_oriented_size * horizontal_dpi / points_per_inch
  EXPECT_EQ("0,0 208x416",
            pwg_raster_converter_->conversion_settings().area.ToString());

  const PrinterProviderPrintJob* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(title, print_job->job_title);
  EXPECT_EQ(base::test::ParseJson(kEmptyPrintTicket), print_job->ticket);
  EXPECT_EQ(kContentTypePWG, print_job->content_type);
  ASSERT_TRUE(print_job->document_bytes);
  EXPECT_EQ(RefCountedMemoryToString(print_data),
            RefCountedMemoryToString(print_job->document_bytes));

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, PrintPwgNonDefaultSettings) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data = base::MakeRefCounted<base::RefCountedBytes>(kPrintData);
  std::u16string title = u"Title";

  extension_printer_handler_->StartPrint(
      title, std::move(base::test::ParseJson(kDuplexSettings).GetDict()),
      print_data,
      base::BindOnce(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  content::RunAllTasksUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  EXPECT_EQ(mojom::DuplexMode::kLongEdge,
            pwg_raster_converter_->bitmap_settings().duplex_mode);
  EXPECT_EQ(TRANSFORM_FLIP_VERTICAL,
            pwg_raster_converter_->bitmap_settings().odd_page_transform);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().reverse_page_order);
  EXPECT_TRUE(pwg_raster_converter_->bitmap_settings().use_color);

  EXPECT_EQ(gfx::Size(200, 100),
            pwg_raster_converter_->conversion_settings().dpi);
  EXPECT_TRUE(pwg_raster_converter_->conversion_settings().autorotate);
  // size = vertically_oriented_size * vertical_dpi / points_per_inch x
  //        horizontally_oriented_size * horizontal_dpi / points_per_inch
  EXPECT_EQ("0,0 138x138",
            pwg_raster_converter_->conversion_settings().area.ToString());

  const PrinterProviderPrintJob* print_job = fake_api->GetNextPendingPrintJob();
  ASSERT_TRUE(print_job);

  EXPECT_EQ(kPrinterId, print_job->printer_id);
  EXPECT_EQ(title, print_job->job_title);
  EXPECT_EQ(base::test::ParseJson(kPrintTicketWithDuplex), print_job->ticket);
  EXPECT_EQ(kContentTypePWG, print_job->content_type);
  ASSERT_TRUE(print_job->document_bytes);
  EXPECT_EQ(RefCountedMemoryToString(print_data),
            RefCountedMemoryToString(print_job->document_bytes));

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(1u, call_count);
  EXPECT_TRUE(success);
  EXPECT_EQ(kPrintRequestSuccess, status);
}

TEST_F(ExtensionPrinterHandlerTest, PrintPwgReset) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data = base::MakeRefCounted<base::RefCountedBytes>(kPrintData);
  std::u16string title = u"Title";

  extension_printer_handler_->StartPrint(
      title, std::move(base::test::ParseJson(kSimpleRasterSettings).GetDict()),
      print_data,
      base::BindOnce(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(0u, call_count);

  content::RunAllTasksUntilIdle();

  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_print_count());

  extension_printer_handler_->Reset();

  fake_api->TriggerNextPrintCallback(kPrintRequestSuccess);

  EXPECT_EQ(0u, call_count);
}

TEST_F(ExtensionPrinterHandlerTest, PrintPwgInvalidTicket) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  auto print_data = base::MakeRefCounted<base::RefCountedBytes>(kPrintData);
  std::u16string title = u"Title";

  extension_printer_handler_->StartPrint(
      title, std::move(base::test::ParseJson(kInvalidSettings).GetDict()),
      print_data,
      base::BindOnce(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(1u, call_count);

  EXPECT_FALSE(success);
  EXPECT_EQ("INVALID_TICKET", status);
}

TEST_F(ExtensionPrinterHandlerTest, PrintPwgFailedConversion) {
  size_t call_count = 0;
  bool success = false;
  std::string status;

  pwg_raster_converter_->FailConversion();

  auto print_data = base::MakeRefCounted<base::RefCountedBytes>(kPrintData);
  std::u16string title = u"Title";

  extension_printer_handler_->StartPrint(
      title, std::move(base::test::ParseJson(kSimpleRasterSettings).GetDict()),
      print_data,
      base::BindOnce(&RecordPrintResult, &call_count, &success, &status));

  EXPECT_EQ(1u, call_count);

  EXPECT_FALSE(success);
  EXPECT_EQ("INVALID_DATA", status);
}

TEST_F(ExtensionPrinterHandlerTest, GrantUsbPrinterAccess) {
  UsbDeviceInfoPtr device =
      fake_usb_manager_.CreateAndAddDevice(0, 0, "Google", "USB Printer", "");
  base::RunLoop().RunUntilIdle();

  size_t call_count = 0;
  base::Value::Dict printer_info;

  std::string printer_id = base::StringPrintf(
      "provisional-usb:fake extension id:%s", device->guid.c_str());
  extension_printer_handler_->StartGrantPrinterAccess(
      printer_id,
      base::BindOnce(&RecordPrinterInfo, &call_count, &printer_info));

  EXPECT_TRUE(printer_info.empty());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_usb_info_count());

  base::Value::Dict original_printer_info =
      base::Value::Dict().Set("id", "printer1").Set("name", "Printer 1");

  fake_api->TriggerNextUsbPrinterInfoCallback(original_printer_info.Clone());

  EXPECT_EQ(1u, call_count);
  EXPECT_FALSE(printer_info.empty());
  EXPECT_EQ(printer_info, original_printer_info)
      << printer_info << ", expected: " << original_printer_info;
}

TEST_F(ExtensionPrinterHandlerTest, GrantUsbPrinterAccessReset) {
  UsbDeviceInfoPtr device =
      fake_usb_manager_.CreateAndAddDevice(0, 0, "Google", "USB Printer", "");
  base::RunLoop().RunUntilIdle();

  size_t call_count = 0;
  base::Value::Dict printer_info;

  extension_printer_handler_->StartGrantPrinterAccess(
      base::StringPrintf("provisional-usb:fake extension id:%s",
                         device->guid.c_str()),
      base::BindOnce(&RecordPrinterInfo, &call_count, &printer_info));

  EXPECT_TRUE(printer_info.empty());
  FakePrinterProviderAPI* fake_api = GetPrinterProviderAPI();
  ASSERT_TRUE(fake_api);
  ASSERT_EQ(1u, fake_api->pending_usb_info_count());

  extension_printer_handler_->Reset();

  base::Value::Dict original_printer_info =
      base::Value::Dict().Set("id", "printer1").Set("name", "Printer 1");

  fake_api->TriggerNextUsbPrinterInfoCallback(std::move(original_printer_info));

  EXPECT_EQ(0u, call_count);
  EXPECT_TRUE(printer_info.empty());
}

}  // namespace printing
