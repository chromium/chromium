// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_provider.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_message_loop.h"
#include "base/version.h"
#include "chromeos/printing/fake_printer_config_cache.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_metadata_manager.h"
#include "chromeos/printing/printer_config_cache.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/remote_ppd_fetcher.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

using PrinterDiscoveryType = PrinterSearchData::PrinterDiscoveryType;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;

// A pseudo-ppd that should get cupsFilter lines extracted from it.
const char kCupsFilterPpdContents[] = R"(
Other random contents that we don't care about.
*cupsFilter: "application/vnd.cups-raster 0 my_filter"
More random contents that we don't care about
*cupsFilter: "application/vnd.cups-awesome 0 a_different_filter"
*cupsFilter: "application/vnd.cups-awesomesauce 0 filter3"
Yet more randome contents that we don't care about.
More random contents that we don't care about.
)";

// A pseudo-ppd that should get cupsFilter2 lines extracted from it.
// We also have cupsFilter lines in here, but since cupsFilter2 lines
// exist, the cupsFilter lines should be ignored.
const char kCupsFilter2PpdContents[] = R"(
Other random contents that we don't care about.
*cupsFilter: "application/vnd.cups-raster 0 my_filter"
More random contents that we don't care about
*cupsFilter2: "foo bar 0 the_real_filter"
*cupsFilter2: "bar baz 381 another_real_filter"
Yet more randome contents that we don't care about.
More random contents that we don't care about.
)";

// Default manufacturers metadata used for these tests.
const char kDefaultManufacturersJson[] = R"({
  "filesMap": {
    "Manufacturer A": "manufacturer_a-en.json",
    "Manufacturer B": "manufacturer_b-en.json"
  }
})";

struct MockRemotePpdFetcher : RemotePpdFetcher {
  MOCK_METHOD(void,
              Fetch,
              (const GURL& url, FetchCallback cb),
              (const, override));
};

// Unowned raw pointers to helper classes composed into the
// PpdProvider at construct time. Used throughout to activate testing
// codepaths.
struct PpdProviderComposedMembers {
  raw_ptr<FakePrinterConfigCache, DanglingUntriaged> config_cache = nullptr;
  raw_ptr<FakePrinterConfigCache, DanglingUntriaged> manager_config_cache =
      nullptr;
  raw_ptr<PpdMetadataManager, DanglingUntriaged> metadata_manager = nullptr;
  raw_ptr<MockRemotePpdFetcher, DanglingUntriaged> remote_ppd_fetcher = nullptr;
};

class PpdProviderTest : public ::testing::Test {
 public:
  // *  Determines where the PpdCache class runs.
  //    * If set to kOnTestThread, the PpdCache class will use the
  //      task environment of the test fixture.
  //    * If set to kInBackgroundThreads, the PpdCache class will
  //      spawn its own background threads.
  //    * Prefer only to run cache on the test thread if you need to
  //      manipulate its sequencing independently of PpdProvider;
  //      otherwise, allowing it spawn its own background threads
  //      should be safe and good for exercising its codepaths.
  enum class PpdCacheRunLocation {
    kOnTestThread,
    kInBackgroundThreads,
  };

  PpdProviderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ASSERT_TRUE(ppd_cache_temp_dir_.CreateUniqueTempDir());
  }

  // Creates and return a provider for a test that uses the given |options|.
  scoped_refptr<PpdProvider> CreateProvider(
      PpdCacheRunLocation where_ppd_cache_runs) {
    switch (where_ppd_cache_runs) {
      case PpdCacheRunLocation::kOnTestThread:
        ppd_cache_ = PpdCache::CreateForTesting(
            ppd_cache_temp_dir_.GetPath(),
            task_environment_.GetMainThreadTaskRunner());
        break;
      case PpdCacheRunLocation::kInBackgroundThreads:
      default:
        ppd_cache_ = PpdCache::Create(ppd_cache_temp_dir_.GetPath());
        break;
    }

    auto manager_config_cache = std::make_unique<FakePrinterConfigCache>();
    provider_backdoor_.manager_config_cache = manager_config_cache.get();

    auto manager = PpdMetadataManager::Create(
        PpdIndexChannel::kProduction, &clock_, std::move(manager_config_cache));
    provider_backdoor_.metadata_manager = manager.get();

    auto config_cache = std::make_unique<FakePrinterConfigCache>();
    provider_backdoor_.config_cache = config_cache.get();

    auto remote_ppd_fetcher = std::make_unique<MockRemotePpdFetcher>();
    provider_backdoor_.remote_ppd_fetcher = remote_ppd_fetcher.get();

    return PpdProvider::Create(base::Version("40.8.6753.09"), ppd_cache_,
                               std::move(manager), std::move(config_cache),
                               std::move(remote_ppd_fetcher));
  }

  // Fills the fake Chrome OS Printing serving root with content.
  // Must be called after CreateProvider().
  void StartFakePpdServer() {
    for (const auto& entry : server_contents()) {
      provider_backdoor_.config_cache->SetFetchResponseForTesting(entry.first,
                                                                  entry.second);
      provider_backdoor_.manager_config_cache->SetFetchResponseForTesting(
          entry.first, entry.second);
    }
  }

  // Interceptor posts a *task* during destruction that actually unregisters
  // things.  So we have to run the message loop post-interceptor-destruction to
  // actually unregister the URLs, otherwise they won't *actually* be
  // unregistered until the next time we invoke the message loop.  Which may be
  // in the middle of the next test.
  //
  // Note this is harmless to call if we haven't started a fake ppd server.
  void StopFakePpdServer() {
    for (const auto& entry : server_contents()) {
      provider_backdoor_.config_cache->Drop(entry.first);
      provider_backdoor_.manager_config_cache->Drop(entry.first);
    }
    task_environment_.RunUntilIdle();
  }

  // Capture the result of a ResolveManufacturers() call.
  void CaptureResolveManufacturers(PpdProvider::CallbackResultCode code,
                                   const std::vector<std::string>& data) {
    captured_resolve_manufacturers_.push_back({code, data});
  }

  // Capture the result of a ResolvePrinters() call.
  void CaptureResolvePrinters(PpdProvider::CallbackResultCode code,
                              const PpdProvider::ResolvedPrintersList& data) {
    captured_resolve_printers_.push_back({code, data});
  }

  // Capture the result of a ResolvePpd() call.
  void CaptureResolvePpd(PpdProvider::CallbackResultCode code,
                         const std::string& ppd_contents) {
    CapturedResolvePpdResults results;
    results.code = code;
    results.ppd_contents = ppd_contents;
    captured_resolve_ppd_.push_back(results);
  }

  // Capture the result of a ResolveUsbIds() call.
  void CaptureResolvePpdReference(PpdProvider::CallbackResultCode code,
                                  const Printer::PpdReference& ref,
                                  const std::string& usb_manufacturer) {
    captured_resolve_ppd_references_.push_back({code, ref, usb_manufacturer});
  }

  // Capture the result of a ResolvePpdLicense() call.
  void CaptureResolvePpdLicense(PpdProvider::CallbackResultCode code,
                                const std::string& license) {
    captured_resolve_ppd_license_.push_back({code, license});
  }

  void CaptureReverseLookup(PpdProvider::CallbackResultCode code,
                            const std::string& manufacturer,
                            const std::string& model) {
    captured_reverse_lookup_.push_back({code, manufacturer, model});
  }

  // Discard the result of a ResolvePpd() call.
  void DiscardResolvePpd(PpdProvider::CallbackResultCode code,
                         const std::string& contents) {}

  void MockRemotePpdFetchResult(const std::string& url, std::string content) {
    auto invoke_callback_with_content =
        [content](RemotePpdFetcher::FetchCallback cb) {
          std::move(cb).Run(RemotePpdFetcher::FetchResultCode::kSuccess,
                            std::move(content));
        };
    EXPECT_CALL(*provider_backdoor_.remote_ppd_fetcher, Fetch(GURL(url), _))
        .WillOnce(Invoke(WithArg<1>(invoke_callback_with_content)));
  }

  void MockRemotePpdFetchResult(const std::string& url,
                                RemotePpdFetcher::FetchResultCode code) {
    auto invoke_callback_with_content =
        [code](RemotePpdFetcher::FetchCallback cb) {
          std::move(cb).Run(code, std::string());
        };
    EXPECT_CALL(*provider_backdoor_.remote_ppd_fetcher, Fetch(GURL(url), _))
        .WillOnce(Invoke(WithArg<1>(invoke_callback_with_content)));
  }

  // Calls the ResolveManufacturer() method of the |provider| and
  // waits for its completion. Ignores the returned string values and
  // returns whether the result code was
  // PpdProvider::CallbackResultCode::SUCCESS.
  bool SuccessfullyResolveManufacturers(PpdProvider* provider) {
    base::RunLoop run_loop;
    PpdProvider::CallbackResultCode code;
    provider->ResolveManufacturers(base::BindLambdaForTesting(
        [&run_loop, &code](
            PpdProvider::CallbackResultCode result_code,
            const std::vector<std::string>& unused_manufacturers) {
          code = result_code;
          run_loop.QuitClosure().Run();
        }));
    run_loop.Run();
    return code == PpdProvider::CallbackResultCode::SUCCESS;
  }

 protected:
  // List of relevant endpoint for this FakeServer
  std::vector<std::pair<std::string, std::string>> server_contents() const {
    // Use brace initialization to express the desired server contents as "url",
    // "contents" pairs.
    return {{"metadata_v3/manufacturers-en.json", kDefaultManufacturersJson},
            {"metadata_v3/manufacturer_a-en.json",
             R"({
                "printers": [ {
                  "name": "printer_a",
                  "emm": "printer_a_ref"
                }, {
                  "name": "printer_b",
                  "emm": "printer_b_ref"
                } ]
               })"},
            {"metadata_v3/manufacturer_b-en.json",
             R"({
                "printers": [ {
                  "name": "printer_c",
                  "emm": "printer_c_ref"
                } ]
               })"},
            {"metadata_v3/index-01.json",
             R"({
                "ppdIndex": {
                  "printer_a_ref": {
                    "ppdMetadata": [ {
                      "name": "printer_a.ppd",
                      "license": "fake_license"
                    } ]
                  }
                }
            })"},
            {"metadata_v3/index-02.json",
             R"({
                "ppdIndex": {
                  "printer_b_ref": {
                    "ppdMetadata": [ {
                      "name": "printer_b.ppd"
                    } ]
                  }
                }
            })"},
            {"metadata_v3/index-03.json",
             R"({
                "ppdIndex": {
                  "printer_c_ref": {
                    "ppdMetadata": [ {
                      "name": "printer_c.ppd"
                    } ]
                  }
                }
            })"},
            {"metadata_v3/index-04.json",
             R"({
                "ppdIndex": {
                  "printer_d_ref": {
                    "ppdMetadata": [ {
                      "name": "printer_d.ppd"
                    } ]
                  }
                }
            })"},
            {"metadata_v3/index-05.json",
             R"({
                "ppdIndex": {
                  "printer_e_ref": {
                    "ppdMetadata": [ {
                      "name": "printer_e.ppd"
                    } ]
                  }
                }
            })"},
            {"metadata_v3/index-08.json",
             R"({
                "ppdIndex": {
                  "Some canonical reference": {
                    "ppdMetadata": [ {
                      "name": "unused.ppd"
                    } ]
                  }
                }
            })"},
            {"metadata_v3/index-10.json",
             R"({
                "ppdIndex": {
                  "Some other canonical reference": {
                    "ppdMetadata": [ {
                      "name": "unused.ppd"
                    } ]
                  },
                  "printer_a_ref_2": {
                    "ppdMetadata": [ {
                      "name": "printer_a.ppd",
                      "license": "fake_license"
                    } ]
                  }
                }
            })"},
            {"metadata_v3/index-15.json",
             R"({
                "ppdIndex": {
                  "zebra zpl label printer": {
                    "ppdMetadata": [ {
                      "name": "unused.ppd"
                    } ]
                  }
                }
            })"},
            {"metadata_v3/usb-031f.json",
             R"({
                "usbIndex": {
                  "1592": {
                    "effectiveMakeAndModel": "Some canonical reference"
                  },
                  "6535": {
                    "effectiveMakeAndModel": "Some other canonical reference"
                  }
                }
            })"},
            {"metadata_v3/usb-03f0.json", ""},
            {"metadata_v3/usb-1234.json", ""},
            {"metadata_v3/usb_vendor_ids.json", R"({
              "entries": [ {
                "vendorId": 799,
                "vendorName": "Seven Ninety Nine LLC"
              }, {
                "vendorId": 1008,
                "vendorName": "HP"
              } ]
            })"},
            {"metadata_v3/reverse_index-en-01.json",
             R"({
                "reverseIndex": {
                  "printer_a_ref": {
                    "manufacturer": "manufacturer_a_en",
                    "model": "printer_a"
                  }
                }
             })"},
            {"metadata_v3/reverse_index-en-10.json",
             R"({
                "reverseIndex": {
                  "printer_a_ref_2": {
                    "manufacturer": "manufacturer_a_en",
                    "model": "printer_a"
                  }
                }
             })"},
            {"metadata_v3/reverse_index-en-19.json",
             R"({
                "reverseIndex": {
                  "unused effective make and model": {
                    "manufacturer": "unused manufacturer",
                    "model": "unused model"
                  }
                }
             })"},
            {"ppds_for_metadata_v3/printer_a.ppd", kCupsFilterPpdContents},
            {"ppds_for_metadata_v3/printer_b.ppd", kCupsFilter2PpdContents},
            {"ppds_for_metadata_v3/printer_c.ppd", "c"},
            {"ppds_for_metadata_v3/printer_d.ppd", "d"},
            {"ppds_for_metadata_v3/printer_e.ppd", "e"},
            {"user_supplied_ppd_directory/user_supplied.ppd", "u"}};
  }

  // Environment for task schedulers.
  base::test::TaskEnvironment task_environment_;

  std::vector<
      std::pair<PpdProvider::CallbackResultCode, std::vector<std::string>>>
      captured_resolve_manufacturers_;

  std::vector<std::pair<PpdProvider::CallbackResultCode,
                        PpdProvider::ResolvedPrintersList>>
      captured_resolve_printers_;

  struct CapturedResolvePpdResults {
    PpdProvider::CallbackResultCode code;
    std::string ppd_contents;
  };
  std::vector<CapturedResolvePpdResults> captured_resolve_ppd_;

  struct CapturedResolvePpdReferenceResults {
    PpdProvider::CallbackResultCode code;
    Printer::PpdReference ref;
    std::string usb_manufacturer;
  };

  std::vector<CapturedResolvePpdReferenceResults>
      captured_resolve_ppd_references_;

  struct CapturedReverseLookup {
    PpdProvider::CallbackResultCode code;
    std::string manufacturer;
    std::string model;
  };
  std::vector<CapturedReverseLookup> captured_reverse_lookup_;

  struct CapturedResolvePpdLicense {
    PpdProvider::CallbackResultCode code;
    std::string license;
  };
  std::vector<CapturedResolvePpdLicense> captured_resolve_ppd_license_;

  base::ScopedTempDir ppd_cache_temp_dir_;
  base::ScopedTempDir interceptor_temp_dir_;

  // Reference to the underlying ppd_cache_ so we can muck with it to test
  // cache-dependent behavior of ppd_provider_.
  scoped_refptr<PpdCache> ppd_cache_;

  PpdProviderComposedMembers provider_backdoor_;

  // Misc extra stuff needed for the test environment to function.
  base::SimpleTestClock clock_;
};

// Test that we get back manufacturer maps as expected.
TEST_F(PpdProviderTest, ManufacturersFetch) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();

  // Issue two requests at the same time, both should be resolved properly.
  provider->ResolveManufacturers(base::BindOnce(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  provider->ResolveManufacturers(base::BindOnce(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(2UL, captured_resolve_manufacturers_.size());
  std::vector<std::string> expected_result(
      {"Manufacturer A", "Manufacturer B"});
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_manufacturers_[0].first);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_manufacturers_[1].first);
  EXPECT_TRUE(captured_resolve_manufacturers_[0].second == expected_result);
  EXPECT_TRUE(captured_resolve_manufacturers_[1].second == expected_result);
}

// Test that we get a reasonable error when we have no server to contact.  Tis
// is almost exactly the same as the above test, we just don't bring up the fake
// server first.
TEST_F(PpdProviderTest, ManufacturersFetchNoServer) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);

  // Issue two requests at the same time, both should resolve properly
  // (though they will fail).
  provider->ResolveManufacturers(base::BindOnce(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  provider->ResolveManufacturers(base::BindOnce(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  task_environment_.FastForwardUntilNoTasksRemain();

  ASSERT_EQ(2UL, captured_resolve_manufacturers_.size());
  EXPECT_EQ(PpdProvider::SERVER_ERROR,
            captured_resolve_manufacturers_[0].first);
  EXPECT_EQ(PpdProvider::SERVER_ERROR,
            captured_resolve_manufacturers_[1].first);
  EXPECT_TRUE(captured_resolve_manufacturers_[0].second.empty());
  EXPECT_TRUE(captured_resolve_manufacturers_[1].second.empty());
}

// Tests that mutiples requests for make-and-model resolution can be fulfilled
// simultaneously.
TEST_F(PpdProviderTest, RepeatedMakeModel) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();

  PrinterSearchData unrecognized_printer;
  unrecognized_printer.discovery_type = PrinterDiscoveryType::kManual;
  unrecognized_printer.make_and_model = {"Printer Printer"};

  PrinterSearchData recognized_printer;
  recognized_printer.discovery_type = PrinterDiscoveryType::kManual;
  recognized_printer.make_and_model = {"printer_a_ref"};

  PrinterSearchData mixed;
  mixed.discovery_type = PrinterDiscoveryType::kManual;
  mixed.make_and_model = {"printer_a_ref", "Printer Printer"};

  // Resolve the same thing repeatedly.
  provider->ResolvePpdReference(
      unrecognized_printer,
      base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                     base::Unretained(this)));
  provider->ResolvePpdReference(
      mixed, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                            base::Unretained(this)));
  provider->ResolvePpdReference(
      recognized_printer,
      base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(static_cast<size_t>(3), captured_resolve_ppd_references_.size());
  EXPECT_EQ(PpdProvider::NOT_FOUND, captured_resolve_ppd_references_[0].code);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_references_[1].code);
  EXPECT_EQ("printer_a_ref",
            captured_resolve_ppd_references_[1].ref.effective_make_and_model);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_references_[2].code);
  EXPECT_EQ("printer_a_ref",
            captured_resolve_ppd_references_[2].ref.effective_make_and_model);
}

// Test successful and unsuccessful usb resolutions.
TEST_F(PpdProviderTest, UsbResolution) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();

  PrinterSearchData search_data;
  search_data.discovery_type = PrinterDiscoveryType::kUsb;

  // Should get back "Some canonical reference"
  search_data.usb_vendor_id = 0x031f;
  search_data.usb_product_id = 1592;
  provider->ResolvePpdReference(
      search_data, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                  base::Unretained(this)));
  // Should get back "Some other canonical reference"
  search_data.usb_vendor_id = 0x031f;
  search_data.usb_product_id = 6535;
  provider->ResolvePpdReference(
      search_data, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                  base::Unretained(this)));

  // Vendor id that exists, nonexistent device id, should get a NOT_FOUND.
  // In our fake serving root, the manufacturer with vendor ID 0x031f
  // (== 799) is named "Seven Ninety Nine LLC."
  search_data.usb_vendor_id = 0x031f;
  search_data.usb_product_id = 8162;
  provider->ResolvePpdReference(
      search_data, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                  base::Unretained(this)));

  // Nonexistent vendor id, should get a NOT_FOUND in the real world, but
  // the URL interceptor we're using considers all nonexistent files to
  // be effectively CONNECTION REFUSED, so we just check for non-success
  // on this one.
  search_data.usb_vendor_id = 0x1234;
  search_data.usb_product_id = 1782;
  provider->ResolvePpdReference(
      search_data, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(captured_resolve_ppd_references_.size(), static_cast<size_t>(4));

  // ResolvePpdReference() takes place in several asynchronous steps, so
  // order is not guaranteed.
  EXPECT_THAT(
      captured_resolve_ppd_references_,
      UnorderedElementsAre(
          AllOf(Field(&CapturedResolvePpdReferenceResults::code,
                      Eq(PpdProvider::SUCCESS)),
                Field(&CapturedResolvePpdReferenceResults::ref,
                      Field(&Printer::PpdReference::effective_make_and_model,
                            StrEq("Some canonical reference")))),
          AllOf(Field(&CapturedResolvePpdReferenceResults::code,
                      Eq(PpdProvider::SUCCESS)),
                Field(&CapturedResolvePpdReferenceResults::ref,
                      Field(&Printer::PpdReference::effective_make_and_model,
                            StrEq("Some other canonical reference")))),
          AllOf(Field(&CapturedResolvePpdReferenceResults::code,
                      Eq(PpdProvider::NOT_FOUND)),
                Field(&CapturedResolvePpdReferenceResults::usb_manufacturer,
                      StrEq("Seven Ninety Nine LLC"))),
          Field(&CapturedResolvePpdReferenceResults::code,
                Eq(PpdProvider::NOT_FOUND))));
}

// Test basic ResolvePrinters() functionality.  At the same time, make
// sure we can get the PpdReference for each of the resolved printers.
TEST_F(PpdProviderTest, ResolvePrinters) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();

  // Required setup calls to advance past PpdProvider's method deferral.
  ASSERT_TRUE(provider_backdoor_.metadata_manager->SetManufacturersForTesting(
      kDefaultManufacturersJson));
  ASSERT_TRUE(SuccessfullyResolveManufacturers(provider.get()));

  provider->ResolvePrinters(
      "Manufacturer A", base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
                                       base::Unretained(this)));
  provider->ResolvePrinters(
      "Manufacturer B", base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
                                       base::Unretained(this)));

  task_environment_.RunUntilIdle();
  ASSERT_EQ(2UL, captured_resolve_printers_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_printers_[0].first);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_printers_[1].first);
  EXPECT_EQ(2UL, captured_resolve_printers_[0].second.size());

  // First capture should get back printer_a, and printer_b, with ppd
  // reference effective make and models of printer_a_ref and printer_b_ref.
  const auto& capture0 = captured_resolve_printers_[0].second;
  ASSERT_EQ(2UL, capture0.size());
  EXPECT_EQ("printer_a", capture0[0].name);
  EXPECT_EQ("printer_a_ref", capture0[0].ppd_ref.effective_make_and_model);

  EXPECT_EQ("printer_b", capture0[1].name);
  EXPECT_EQ("printer_b_ref", capture0[1].ppd_ref.effective_make_and_model);

  // Second capture should get back printer_c with effective make and model of
  // printer_c_ref
  const auto& capture1 = captured_resolve_printers_[1].second;
  ASSERT_EQ(1UL, capture1.size());
  EXPECT_EQ("printer_c", capture1[0].name);
  EXPECT_EQ("printer_c_ref", capture1[0].ppd_ref.effective_make_and_model);
}

// Test that if we give a bad reference to ResolvePrinters(), we get a
// SERVER_ERROR. There's currently no feedback that indicates
// specifically to the caller that they asked for the printers of
// a manufacturer we didn't previously advertise.
TEST_F(PpdProviderTest, ResolvePrintersBadReference) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();

  // Required setup calls to advance past PpdProvider's method deferral.
  ASSERT_TRUE(provider_backdoor_.metadata_manager->SetManufacturersForTesting(
      kDefaultManufacturersJson));
  ASSERT_TRUE(SuccessfullyResolveManufacturers(provider.get()));

  provider->ResolvePrinters(
      "bogus_doesnt_exist",
      base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1UL, captured_resolve_printers_.size());
  EXPECT_EQ(PpdProvider::SERVER_ERROR, captured_resolve_printers_[0].first);
}

// Test that if the server is unavailable, we get SERVER_ERRORs back out.
TEST_F(PpdProviderTest, ResolvePrintersNoServer) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);

  // Required setup calls to advance past PpdProvider's method deferral.
  ASSERT_TRUE(provider_backdoor_.metadata_manager->SetManufacturersForTesting(
      kDefaultManufacturersJson));
  ASSERT_TRUE(SuccessfullyResolveManufacturers(provider.get()));

  provider->ResolvePrinters(
      "Manufacturer A", base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
                                       base::Unretained(this)));
  provider->ResolvePrinters(
      "Manufacturer B", base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
                                       base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(2UL, captured_resolve_printers_.size());
  EXPECT_EQ(PpdProvider::SERVER_ERROR, captured_resolve_printers_[0].first);
  EXPECT_EQ(PpdProvider::SERVER_ERROR, captured_resolve_printers_[1].first);
}

// Test a successful ppd resolution from an effective_make_and_model reference.
TEST_F(PpdProviderTest, ResolveServerKeyPpd) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();
  Printer::PpdReference ref;
  ref.effective_make_and_model = "printer_b_ref";
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  ref.effective_make_and_model = "printer_c_ref";
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(2UL, captured_resolve_ppd_.size());

  // ResolvePpd() works in several asynchronous steps, so order of
  // return is not guaranteed.
  EXPECT_THAT(
      captured_resolve_ppd_,
      UnorderedElementsAre(
          AllOf(Field(&CapturedResolvePpdResults::code,
                      PpdProvider::CallbackResultCode::SUCCESS),
                Field(&CapturedResolvePpdResults::ppd_contents,
                      StrEq(kCupsFilter2PpdContents))),
          AllOf(Field(&CapturedResolvePpdResults::code,
                      PpdProvider::CallbackResultCode::SUCCESS),
                Field(&CapturedResolvePpdResults::ppd_contents, StrEq("c")))));
}

// Test a successful ppd resolution from a user_supplied_url field when
// reading from a file.  Note we shouldn't need the server to be up
// to do this successfully, as we should be able to do this offline.
TEST_F(PpdProviderTest, ResolveUserSuppliedUrlPpdFromFile) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath filename = temp_dir.GetPath().Append("my_spiffy.ppd");

  std::string user_ppd_contents = "Woohoo";

  ASSERT_TRUE(base::WriteFile(filename, user_ppd_contents));

  Printer::PpdReference ref;
  ref.user_supplied_ppd_url =
      base::StringPrintf("file://%s", filename.MaybeAsASCII().c_str());
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(user_ppd_contents, captured_resolve_ppd_[0].ppd_contents);
}

// Test that we cache ppd resolutions when we fetch them and that we can resolve
// from the cache without the server available.
TEST_F(PpdProviderTest, ResolvedPpdsGetCached) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  std::string user_ppd_contents = "Woohoo";
  Printer::PpdReference ref;
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath filename = temp_dir.GetPath().Append("my_spiffy.ppd");

    ASSERT_TRUE(base::WriteFile(filename, user_ppd_contents));

    ref.user_supplied_ppd_url =
        base::StringPrintf("file://%s", filename.MaybeAsASCII().c_str());
    provider->ResolvePpd(ref,
                         base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                        base::Unretained(this)));
    task_environment_.RunUntilIdle();

    ASSERT_EQ(1UL, captured_resolve_ppd_.size());
    EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
    EXPECT_EQ(user_ppd_contents, captured_resolve_ppd_[0].ppd_contents);
  }
  // ScopedTempDir goes out of scope, so the source file should now be
  // deleted.  But if we resolve again, we should hit the cache and
  // still be successful.

  captured_resolve_ppd_.clear();

  // Recreate the provider to make sure we don't have any memory caches which
  // would mask problems with disk persistence.
  provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);

  // Re-resolve.
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(user_ppd_contents, captured_resolve_ppd_[0].ppd_contents);
}

// Test that all entrypoints will correctly work with case-insensitve
// effective-make-and-model strings.
TEST_F(PpdProviderTest, CaseInsensitiveMakeAndModel) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();
  std::string ref = "pRiNteR_A_reF";

  Printer::PpdReference ppd_ref;
  ppd_ref.effective_make_and_model = ref;
  provider->ResolvePpd(ppd_ref,
                       base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                      base::Unretained(this)));
  provider->ReverseLookup(ref,
                          base::BindOnce(&PpdProviderTest::CaptureReverseLookup,
                                         base::Unretained(this)));
  PrinterSearchData printer_info;
  printer_info.make_and_model = {ref};
  provider->ResolvePpdReference(
      printer_info, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                   base::Unretained(this)));
  task_environment_.RunUntilIdle();

  // Check PpdProvider::ResolvePpd
  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(kCupsFilterPpdContents, captured_resolve_ppd_[0].ppd_contents);

  // Check PpdProvider::ReverseLookup
  ASSERT_EQ(1UL, captured_reverse_lookup_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_reverse_lookup_[0].code);
  EXPECT_EQ("manufacturer_a_en", captured_reverse_lookup_[0].manufacturer);
  EXPECT_EQ("printer_a", captured_reverse_lookup_[0].model);

  // Check PpdProvider::ResolvePpdReference
  ASSERT_EQ(1UL, captured_resolve_ppd_references_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_references_[0].code);
  EXPECT_EQ("printer_a_ref",
            captured_resolve_ppd_references_[0].ref.effective_make_and_model);
}

// Test that ResolvePpd is able to correctly retrieve PPD content for the given
// not-primary effective make and model.
TEST_F(PpdProviderTest, ResolvePpdFromSecondaryMakeAndModel) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();
  std::string ref = "pRiNteR_A_reF_2";

  Printer::PpdReference ppd_ref;
  ppd_ref.effective_make_and_model = ref;
  provider->ResolvePpd(ppd_ref,
                       base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                      base::Unretained(this)));
  task_environment_.RunUntilIdle();

  // Check PpdProvider::ResolvePpd
  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(kCupsFilterPpdContents, captured_resolve_ppd_[0].ppd_contents);
}

// Tests that ResolvePpdLicense is able to correctly source the index and
// determine the name of the PPD license associated with the given effective
// make and model (if any).
TEST_F(PpdProviderTest, ResolvePpdLicense) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();

  // For this effective_make_and_model, we expect that there is associated
  // license.
  const char kEmm1[] = "printer_a_ref";
  provider->ResolvePpdLicense(
      kEmm1, base::BindOnce(&PpdProviderTest::CaptureResolvePpdLicense,
                            base::Unretained(this)));

  // We do not expect there to be any license associated with this
  // effective_make_and_model, so the response should be empty.
  const char kEmm2[] = "printer_b_ref";
  provider->ResolvePpdLicense(
      kEmm2, base::BindOnce(&PpdProviderTest::CaptureResolvePpdLicense,
                            base::Unretained(this)));

  task_environment_.RunUntilIdle();

  ASSERT_EQ(2UL, captured_resolve_ppd_license_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_license_[0].code);
  EXPECT_EQ("fake_license", captured_resolve_ppd_license_[0].license);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_license_[1].code);
  EXPECT_EQ("", captured_resolve_ppd_license_[1].license);
}

// Tests that ResolvePpdLicense is able to correctly source the index and
// determine the name of the PPD license associated with the given not-primary
// effective make and model.
TEST_F(PpdProviderTest, ResolvePpdLicenseFromSecondaryMakeAndModel) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();

  // For this effective_make_and_model, we expect that there is associated
  // license.
  const char kEmm1[] = "printer_A_ref_2";
  provider->ResolvePpdLicense(
      kEmm1, base::BindOnce(&PpdProviderTest::CaptureResolvePpdLicense,
                            base::Unretained(this)));

  task_environment_.RunUntilIdle();

  ASSERT_EQ(1UL, captured_resolve_ppd_license_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_license_[0].code);
  EXPECT_EQ("fake_license", captured_resolve_ppd_license_[0].license);
}

// Verifies that we can extract the Manufacturer and Model selection for a
// given effective make and model.
TEST_F(PpdProviderTest, ReverseLookup) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();
  std::string ref = "printer_a_ref";
  provider->ReverseLookup(ref,
                          base::BindOnce(&PpdProviderTest::CaptureReverseLookup,
                                         base::Unretained(this)));
  // TODO(skau): PpdProvider has a race condition that prevents running these
  // requests in parallel.
  task_environment_.RunUntilIdle();

  std::string ref_fail = "printer_does_not_exist";
  provider->ReverseLookup(ref_fail,
                          base::BindOnce(&PpdProviderTest::CaptureReverseLookup,
                                         base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(2U, captured_reverse_lookup_.size());
  CapturedReverseLookup success_capture = captured_reverse_lookup_[0];
  EXPECT_EQ(PpdProvider::SUCCESS, success_capture.code);
  EXPECT_EQ("manufacturer_a_en", success_capture.manufacturer);
  EXPECT_EQ("printer_a", success_capture.model);

  CapturedReverseLookup failed_capture = captured_reverse_lookup_[1];
  EXPECT_EQ(PpdProvider::NOT_FOUND, failed_capture.code);
}

// Verifies that we can extract the Manufacturer and Model selection for a
// given not-primary effective make and model.
TEST_F(PpdProviderTest, ReverseLookupFromSecondaryMakeAndModel) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();
  std::string ref = "printer_A_ref_2";
  provider->ReverseLookup(ref,
                          base::BindOnce(&PpdProviderTest::CaptureReverseLookup,
                                         base::Unretained(this)));

  task_environment_.RunUntilIdle();

  ASSERT_EQ(1U, captured_reverse_lookup_.size());
  CapturedReverseLookup success_capture = captured_reverse_lookup_[0];
  EXPECT_EQ(PpdProvider::SUCCESS, success_capture.code);
  EXPECT_EQ("manufacturer_a_en", success_capture.manufacturer);
  EXPECT_EQ("printer_a", success_capture.model);
}

// Verifies that we never attempt to re-download a PPD that we
// previously retrieved from the serving root. The Chrome OS Printing
// Team plans to keep PPDs immutable inside the serving root, so
// PpdProvider should always prefer to retrieve a PPD from the PpdCache
// when it's possible to do so.
TEST_F(PpdProviderTest, PreferToResolvePpdFromPpdCacheOverServingRoot) {
  // Explicitly *not* starting a fake server.
  std::string cached_ppd_contents =
      "These cached contents are different from what's being served";
  auto provider = CreateProvider(PpdCacheRunLocation::kOnTestThread);
  Printer::PpdReference ref;
  ref.effective_make_and_model = "printer_a_ref";
  std::string cache_key = PpdProvider::PpdReferenceToCacheKey(ref);

  // Cache exists, and is just created, so should be fresh.
  //
  // PPD basename is taken from value specified in forward index shard
  // defined in server_contents().
  const std::string ppd_basename = "printer_a.ppd";
  ppd_cache_->StoreForTesting(PpdProvider::PpdBasenameToCacheKey(ppd_basename),
                              cached_ppd_contents, base::TimeDelta());
  ppd_cache_->StoreForTesting(PpdProvider::PpdReferenceToCacheKey(ref),
                              ppd_basename, base::TimeDelta());
  task_environment_.RunUntilIdle();
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1UL, captured_resolve_ppd_.size());

  // Should get the cached (not served) results back, and not have hit the
  // network.
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(cached_ppd_contents, captured_resolve_ppd_[0].ppd_contents);
}

// For user-provided ppds, we should always use the latest version on
// disk if it still exists there.
TEST_F(PpdProviderTest, UserPpdAlwaysRefreshedIfAvailable) {
  base::ScopedTempDir temp_dir;
  std::string cached_ppd_contents = "Cached Ppd Contents";
  std::string disk_ppd_contents = "Updated Ppd Contents";
  auto provider = CreateProvider(PpdCacheRunLocation::kOnTestThread);
  StartFakePpdServer();
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath filename = temp_dir.GetPath().Append("my_spiffy.ppd");

  Printer::PpdReference ref;
  ref.user_supplied_ppd_url =
      base::StringPrintf("file://%s", filename.MaybeAsASCII().c_str());

  // Put cached_ppd_contents into the cache.
  ppd_cache_->StoreForTesting(PpdProvider::PpdReferenceToCacheKey(ref),
                              cached_ppd_contents, base::TimeDelta());
  task_environment_.RunUntilIdle();

  // Write different contents to disk, so that the cached contents are
  // now stale.
  ASSERT_TRUE(base::WriteFile(filename, disk_ppd_contents));

  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(disk_ppd_contents, captured_resolve_ppd_[0].ppd_contents);

  // Check that the cache was also updated with the new contents.
  PpdCache::FindResult captured_find_result;
  ppd_cache_->Find(PpdProvider::PpdReferenceToCacheKey(ref),
                   base::BindOnce(
                       [](PpdCache::FindResult* captured_find_result,
                          const PpdCache::FindResult& find_result) {
                         *captured_find_result = find_result;
                       },
                       &captured_find_result));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_result.success, true);
  EXPECT_EQ(captured_find_result.contents, disk_ppd_contents);
}

// Test resolving usb manufacturer when failed to resolve PpdReference.
TEST_F(PpdProviderTest, ResolveUsbManufacturer) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();

  PrinterSearchData search_data;
  search_data.discovery_type = PrinterDiscoveryType::kUsb;

  // Vendor id that exists, nonexistent device id, should get a NOT_FOUND.
  // Although this is an unsupported printer model, we can still expect to get
  // the manufacturer name.
  search_data.usb_vendor_id = 0x03f0;
  search_data.usb_product_id = 9999;
  provider->ResolvePpdReference(
      search_data, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                  base::Unretained(this)));

  // Nonexistent vendor id, should get a NOT_FOUND in the real world, but
  // the URL interceptor we're using considers all nonexistent files to
  // be effectively CONNECTION REFUSED, so we just check for non-success
  // on this one. We should also not be able to get a manufacturer name from a
  // nonexistent vendor id.
  search_data.usb_vendor_id = 0x1234;
  search_data.usb_product_id = 1782;
  provider->ResolvePpdReference(
      search_data, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(static_cast<size_t>(2), captured_resolve_ppd_references_.size());
  // Was able to find the printer manufactuer but unable to find the printer
  // model should result in a NOT_FOUND.
  EXPECT_EQ(PpdProvider::NOT_FOUND, captured_resolve_ppd_references_[0].code);
  // Failed to grab the PPD for a USB printer, but should still be able to grab
  // its manufacturer name.
  EXPECT_EQ("HP", captured_resolve_ppd_references_[0].usb_manufacturer);

  // Unable to find the PPD file should result in a failure.
  EXPECT_FALSE(captured_resolve_ppd_references_[1].code ==
               PpdProvider::SUCCESS);
  // Unknown vendor id should result in an empty manufacturer string.
  EXPECT_TRUE(captured_resolve_ppd_references_[1].usb_manufacturer.empty());
}

TEST_F(PpdProviderTest, GenericZebraPpdResolution) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  StartFakePpdServer();

  PrinterSearchData search_data;
  search_data.discovery_type = PrinterDiscoveryType::kManual;
  // Zebra and ZPL tell the resolver to search for the generic PPD, which
  // should be found.
  search_data.printer_id.set_make("Zebra");
  search_data.printer_id.set_model("ZTC label printer 2000 ZPL");

  provider->ResolvePpdReference(
      search_data, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                  base::Unretained(this)));

  task_environment_.RunUntilIdle();

  // Some Zebra printers use a different manufacturer.  Test that.
  search_data.printer_id.set_make("Zebra Technologies");

  provider->ResolvePpdReference(
      search_data, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                  base::Unretained(this)));

  task_environment_.RunUntilIdle();

  // No ZPL in the model name, so this PPD will not be found.
  search_data.printer_id.set_model("ZTC label printer 4000");
  provider->ResolvePpdReference(
      search_data, base::BindOnce(&PpdProviderTest::CaptureResolvePpdReference,
                                  base::Unretained(this)));

  task_environment_.RunUntilIdle();

  ASSERT_EQ(3UL, captured_resolve_ppd_references_.size());

  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_references_[0].code);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_references_[1].code);
  EXPECT_EQ(PpdProvider::NOT_FOUND, captured_resolve_ppd_references_[2].code);
}

TEST_F(PpdProviderTest, RemotePpdFetchedFromUrlIfAvailable) {
  auto provider = CreateProvider(PpdCacheRunLocation::kOnTestThread);
  Printer::PpdReference ref;
  ref.user_supplied_ppd_url = "https://ppd-url";
  ppd_cache_->StoreForTesting(PpdProvider::PpdReferenceToCacheKey(ref),
                              "cached-content", base::TimeDelta());
  MockRemotePpdFetchResult("https://ppd-url", "ppd-content");

  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(captured_resolve_ppd_[0].ppd_contents, "ppd-content");
}

TEST_F(PpdProviderTest, RemotePpdResolveUsesCacheIfFetchFails) {
  auto provider = CreateProvider(PpdCacheRunLocation::kOnTestThread);
  Printer::PpdReference ref;
  ref.user_supplied_ppd_url = "https://ppd-url";
  ppd_cache_->StoreForTesting(PpdProvider::PpdReferenceToCacheKey(ref),
                              "cached-content", base::TimeDelta());
  MockRemotePpdFetchResult("https://ppd-url",
                           RemotePpdFetcher::FetchResultCode::kNetworkError);

  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(captured_resolve_ppd_[0].ppd_contents, "cached-content");
}

TEST_F(PpdProviderTest, RemotePpdResolveFailureResultsInServerError) {
  auto provider = CreateProvider(PpdCacheRunLocation::kInBackgroundThreads);
  Printer::PpdReference ref;
  ref.user_supplied_ppd_url = "https://ppd-url";
  MockRemotePpdFetchResult("https://ppd-url",
                           RemotePpdFetcher::FetchResultCode::kNetworkError);

  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SERVER_ERROR, captured_resolve_ppd_[0].code);
  EXPECT_TRUE(captured_resolve_ppd_[0].ppd_contents.empty());
}

// Test that PPD result codes have the expected names.
TEST_F(PpdProviderTest, ResultCodeNames) {
  EXPECT_EQ(PpdProvider::CallbackResultCodeName(PpdProvider::SUCCESS),
            "SUCCESS");
  EXPECT_EQ(PpdProvider::CallbackResultCodeName(PpdProvider::NOT_FOUND),
            "NOT_FOUND");
  EXPECT_EQ(PpdProvider::CallbackResultCodeName(PpdProvider::SERVER_ERROR),
            "SERVER_ERROR");
  EXPECT_EQ(PpdProvider::CallbackResultCodeName(PpdProvider::INTERNAL_ERROR),
            "INTERNAL_ERROR");
  EXPECT_EQ(PpdProvider::CallbackResultCodeName(PpdProvider::PPD_TOO_LARGE),
            "PPD_TOO_LARGE");
}

}  // namespace
}  // namespace chromeos
