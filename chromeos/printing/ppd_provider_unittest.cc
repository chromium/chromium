// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_provider.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_message_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/printer_configuration.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using PrinterDiscoveryType = PrinterSearchData::PrinterDiscoveryType;

// Name of the fake server we're resolving ppds from.
const char kPpdServer[] = "bogus.google.com";

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

class PpdProviderTest : public ::testing::Test {
 public:
  PpdProviderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    TurnOffFakePpdServer();
  }

  void SetUp() override {
    ASSERT_TRUE(ppd_cache_temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { StopFakePpdServer(); }

  // Create and return a provider for a test that uses the given |locale|.  If
  // run_cache_on_test_thread is true, we'll run the cache using the
  // task_environment_; otherwise we'll let it spawn it's own background
  // threads.  You should only run the cache on the test thread if you need to
  // explicity drain cache actions independently of draining ppd provider
  // actions; otherwise letting the cache spawn its own thread should be safe,
  // and better exercises the code paths under test.
  scoped_refptr<PpdProvider> CreateProvider(const std::string& locale,
                                            bool run_cache_on_test_thread) {
    auto provider_options = PpdProvider::Options();
    provider_options.ppd_server_root = std::string("https://") + kPpdServer;

    if (run_cache_on_test_thread) {
      ppd_cache_ = PpdCache::CreateForTesting(
          ppd_cache_temp_dir_.GetPath(),
          task_environment_.GetMainThreadTaskRunner());
    } else {
      ppd_cache_ = PpdCache::Create(ppd_cache_temp_dir_.GetPath());
    }
    return PpdProvider::Create(
        locale, base::BindLambdaForTesting([this]() {
          // Casting here is necessary b/c RepeatingCallback needs help
          // coercing types.
          // Casting is safe since its a guaranteed upcast.
          return dynamic_cast<network::mojom::URLLoaderFactory*>(
              &loader_factory_);
        }),
        ppd_cache_, base::Version("40.8.6753.09"), provider_options);
  }

  // Fill loader_factory_ with preset contents for test URLs
  void StartFakePpdServer() {
    loader_factory_.ClearResponses();
    for (const auto& entry : server_contents()) {
      network::URLLoaderCompletionStatus status;
      if (!entry.second.empty()) {
        status.decoded_body_length = entry.second.size();
      } else {
        status.error_code = net::ERR_ADDRESS_UNREACHABLE;
      }

      loader_factory_.AddResponse(FileToURL(entry.first),
                                  network::mojom::URLResponseHead::New(),
                                  entry.second, status);
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
    TurnOffFakePpdServer();
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

 protected:
  // Run a ResolveManufacturers run from the given locale, expect to get
  // results in expected_used_locale.
  void RunLocalizationTest(const std::string& browser_locale,
                           const std::string& expected_used_locale) {
    captured_resolve_manufacturers_.clear();
    auto provider = CreateProvider(browser_locale, false);
    provider->ResolveManufacturers(base::BindOnce(
        &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
    task_environment_.RunUntilIdle();
    provider = nullptr;
    ASSERT_EQ(captured_resolve_manufacturers_.size(), 1UL);
    EXPECT_EQ(captured_resolve_manufacturers_[0].first, PpdProvider::SUCCESS);

    const auto& result_vec = captured_resolve_manufacturers_[0].second;

    // It's sufficient to check for one of the expected locale keys to make sure
    // we got the right map.
    EXPECT_TRUE(
        base::Contains(result_vec, "manufacturer_a_" + expected_used_locale));
  }

  // Fake server being down, return ERR_ADDRESS_UNREACHABLE for all endpoints
  void TurnOffFakePpdServer() {
    loader_factory_.ClearResponses();
    network::URLLoaderCompletionStatus status(net::ERR_ADDRESS_UNREACHABLE);
    for (const auto& entry : server_contents()) {
      loader_factory_.AddResponse(FileToURL(entry.first),
                                  network::mojom::URLResponseHead::New(), "",
                                  status);
    }
  }

  GURL FileToURL(std::string filename) {
    return GURL(
        base::StringPrintf("https://%s/%s", kPpdServer, filename.c_str()));
  }

  // List of relevant endpoint for this FakeServer
  std::vector<std::pair<std::string, std::string>> server_contents() const {
    // Use brace initialization to express the desired server contents as "url",
    // "contents" pairs.
    return {{"metadata_v2/locales.json",
             R"(["en",
             "es-mx",
             "en-gb"])"},
            {"metadata_v2/index-01.json",
             R"([
             ["printer_a_ref", "printer_a.ppd", {"license": "fake_license"}]
            ])"},
            {"metadata_v2/index-02.json",
             R"([
             ["printer_b_ref", "printer_b.ppd"]
            ])"},
            {"metadata_v2/index-03.json",
             R"([
             ["printer_c_ref", "printer_c.ppd"]
            ])"},
            {"metadata_v2/index-04.json",
             R"([
             ["printer_d_ref", "printer_d.ppd"]
            ])"},
            {"metadata_v2/index-05.json",
             R"([
             ["printer_e_ref", "printer_e.ppd"]
            ])"},
            {"metadata_v2/index-13.json",
             R"([
            ])"},
            {"metadata_v2/usb-031f.json",
             R"([
             [1592, "Some canonical reference"],
             [6535, "Some other canonical reference"]
            ])"},
            {"metadata_v2/usb-03f0.json", ""},
            {"metadata_v2/usb-1234.json", ""},
            {"metadata_v2/manufacturers-en.json",
             R"([
            ["manufacturer_a_en", "manufacturer_a.json"],
            ["manufacturer_b_en", "manufacturer_b.json"]
            ])"},
            {"metadata_v2/manufacturers-en-gb.json",
             R"([
            ["manufacturer_a_en-gb", "manufacturer_a.json"],
            ["manufacturer_b_en-gb", "manufacturer_b.json"]
            ])"},
            {"metadata_v2/manufacturers-es-mx.json",
             R"([
            ["manufacturer_a_es-mx", "manufacturer_a.json"],
            ["manufacturer_b_es-mx", "manufacturer_b.json"]
            ])"},
            {"metadata_v2/manufacturer_a.json",
             R"([
            ["printer_a", "printer_a_ref"],
            ["printer_b", "printer_b_ref"],
            ["printer_d", "printer_d_ref"]
            ])"},
            {"metadata_v2/manufacturer_a.json",
             R"([
            ["printer_a", "printer_a_ref",
              {"min_milestone":25.0000}],
            ["printer_b", "printer_b_ref",
              {"min_milestone":30.0000, "max_milestone":45.0000}],
            ["printer_d", "printer_d_ref",
              {"min_milestone":60.0000, "max_milestone":75.0000}]
            ])"},
            {"metadata_v2/manufacturer_b.json",
             R"([
            ["printer_c", "printer_c_ref"],
            ["printer_e", "printer_e_ref"]
            ])"},
            {"metadata_v2/reverse_index-en-01.json",
             R"([
             ["printer_a_ref", "manufacturer_a_en", "printer_a"]
             ])"},
            {"metadata_v2/reverse_index-en-19.json",
             R"([
             ])"},
            {"metadata_v2/manufacturer_b.json",
             R"([
            ["printer_c", "printer_c_ref",
              {"max_milestone":55.0000}],
            ["printer_e", "printer_e_ref",
              {"min_milestone":17.0000, "max_milestone":33.0000}]
            ])"},
            {"ppds/printer_a.ppd", kCupsFilterPpdContents},
            {"ppds/printer_b.ppd", kCupsFilter2PpdContents},
            {"ppds/printer_c.ppd", "c"},
            {"ppds/printer_d.ppd", "d"},
            {"ppds/printer_e.ppd", "e"},
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

  // Misc extra stuff needed for the test environment to function.
  //  base::TestMessageLoop loop_;
  network::TestURLLoaderFactory loader_factory_;
};

// Test that we get back manufacturer maps as expected.
TEST_F(PpdProviderTest, ManufacturersFetch) {
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);
  // Issue two requests at the same time, both should be resolved properly.
  provider->ResolveManufacturers(base::BindOnce(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  provider->ResolveManufacturers(base::BindOnce(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(2UL, captured_resolve_manufacturers_.size());
  std::vector<std::string> expected_result(
      {"manufacturer_a_en", "manufacturer_b_en"});
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_manufacturers_[0].first);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_manufacturers_[1].first);
  EXPECT_TRUE(captured_resolve_manufacturers_[0].second == expected_result);
  EXPECT_TRUE(captured_resolve_manufacturers_[1].second == expected_result);
}

// Test that we get a reasonable error when we have no server to contact.  Tis
// is almost exactly the same as the above test, we just don't bring up the fake
// server first.
TEST_F(PpdProviderTest, ManufacturersFetchNoServer) {
  auto provider = CreateProvider("en", false);
  // Issue two requests at the same time, both should be resolved properly.
  provider->ResolveManufacturers(base::BindOnce(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  provider->ResolveManufacturers(base::BindOnce(
      &PpdProviderTest::CaptureResolveManufacturers, base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(2UL, captured_resolve_manufacturers_.size());
  EXPECT_EQ(PpdProvider::SERVER_ERROR,
            captured_resolve_manufacturers_[0].first);
  EXPECT_EQ(PpdProvider::SERVER_ERROR,
            captured_resolve_manufacturers_[1].first);
  EXPECT_TRUE(captured_resolve_manufacturers_[0].second.empty());
  EXPECT_TRUE(captured_resolve_manufacturers_[1].second.empty());
}

// Test that we get things in the requested locale, and that fallbacks are sane.
TEST_F(PpdProviderTest, LocalizationAndFallbacks) {
  StartFakePpdServer();
  RunLocalizationTest("en-gb", "en-gb");
  RunLocalizationTest("en-blah", "en");
  RunLocalizationTest("en-gb-foo", "en-gb");
  RunLocalizationTest("es", "es-mx");
  RunLocalizationTest("bogus", "en");
}

// Tests that mutiples requests for make-and-model resolution can be fulfilled
// simultaneously.
TEST_F(PpdProviderTest, RepeatedMakeModel) {
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);

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
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);

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
  EXPECT_EQ(captured_resolve_ppd_references_[0].code, PpdProvider::SUCCESS);
  EXPECT_EQ(captured_resolve_ppd_references_[0].ref.effective_make_and_model,
            "Some canonical reference");
  EXPECT_EQ(captured_resolve_ppd_references_[1].code, PpdProvider::SUCCESS);
  EXPECT_EQ(captured_resolve_ppd_references_[1].ref.effective_make_and_model,
            "Some other canonical reference");
  EXPECT_EQ(captured_resolve_ppd_references_[2].code, PpdProvider::NOT_FOUND);
  EXPECT_FALSE(captured_resolve_ppd_references_[3].code ==
               PpdProvider::SUCCESS);
}

// For convenience a null ResolveManufacturers callback target.
void ResolveManufacturersNop(PpdProvider::CallbackResultCode code,
                             const std::vector<std::string>& v) {}

// Test basic ResolvePrinters() functionality.  At the same time, make
// sure we can get the PpdReference for each of the resolved printers.
TEST_F(PpdProviderTest, ResolvePrinters) {
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);

  // Grab the manufacturer list, but don't bother to save it, we know what
  // should be in it and we check that elsewhere.  We just need to run the
  // resolve to populate the internal PpdProvider structures.
  provider->ResolveManufacturers(base::BindOnce(&ResolveManufacturersNop));
  task_environment_.RunUntilIdle();

  provider->ResolvePrinters(
      "manufacturer_a_en",
      base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
                     base::Unretained(this)));
  provider->ResolvePrinters(
      "manufacturer_b_en",
      base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
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
  // EXPECT_EQ(base::Version("55"), capture1[0].restrictions.max_milestone);
}

// Test that if we give a bad reference to ResolvePrinters(), we get an
// INTERNAL_ERROR.
TEST_F(PpdProviderTest, ResolvePrintersBadReference) {
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);
  provider->ResolveManufacturers(base::BindOnce(&ResolveManufacturersNop));
  task_environment_.RunUntilIdle();

  provider->ResolvePrinters(
      "bogus_doesnt_exist",
      base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1UL, captured_resolve_printers_.size());
  EXPECT_EQ(PpdProvider::INTERNAL_ERROR, captured_resolve_printers_[0].first);
}

// Test that if the server is unavailable, we get SERVER_ERRORs back out.
TEST_F(PpdProviderTest, ResolvePrintersNoServer) {
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);
  provider->ResolveManufacturers(base::BindOnce(&ResolveManufacturersNop));
  task_environment_.RunUntilIdle();

  StopFakePpdServer();

  provider->ResolvePrinters(
      "manufacturer_a_en",
      base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
                     base::Unretained(this)));
  provider->ResolvePrinters(
      "manufacturer_b_en",
      base::BindOnce(&PpdProviderTest::CaptureResolvePrinters,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(2UL, captured_resolve_printers_.size());
  EXPECT_EQ(PpdProvider::SERVER_ERROR, captured_resolve_printers_[0].first);
  EXPECT_EQ(PpdProvider::SERVER_ERROR, captured_resolve_printers_[1].first);
}

// Test a successful ppd resolution from an effective_make_and_model reference.
TEST_F(PpdProviderTest, ResolveServerKeyPpd) {
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);
  Printer::PpdReference ref;
  ref.effective_make_and_model = "printer_b_ref";
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  ref.effective_make_and_model = "printer_c_ref";
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(2UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(kCupsFilter2PpdContents, captured_resolve_ppd_[0].ppd_contents);
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[1].code);
  EXPECT_EQ("c", captured_resolve_ppd_[1].ppd_contents);
}

// Test that we *don't* resolve a ppd URL over non-file schemes.  It's not clear
// whether we'll want to do this in the long term, but for now this is
// disallowed because we're not sure we completely understand the security
// implications.
TEST_F(PpdProviderTest, ResolveUserSuppliedUrlPpdFromNetworkFails) {
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);

  Printer::PpdReference ref;
  ref.user_supplied_ppd_url = base::StringPrintf(
      "https://%s/user_supplied_ppd_directory/user_supplied.ppd", kPpdServer);
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1UL, captured_resolve_ppd_.size());
  EXPECT_EQ(PpdProvider::INTERNAL_ERROR, captured_resolve_ppd_[0].code);
  EXPECT_TRUE(captured_resolve_ppd_[0].ppd_contents.empty());
}

// Test a successful ppd resolution from a user_supplied_url field when
// reading from a file.  Note we shouldn't need the server to be up
// to do this successfully, as we should be able to do this offline.
TEST_F(PpdProviderTest, ResolveUserSuppliedUrlPpdFromFile) {
  auto provider = CreateProvider("en", false);
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
  auto provider = CreateProvider("en", false);
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
  provider = CreateProvider("en", false);

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
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);
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

// Tests that ResolvePpdLicense is able to correctly source the index and
// determine the name of the PPD license associated with the given effecive make
// and model (if any).
TEST_F(PpdProviderTest, ResolvePpdLicense) {
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);

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

// Verifies that we can extract the Manufacturer and Model selectison for a
// given effective make and model.
TEST_F(PpdProviderTest, ReverseLookup) {
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);
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

// If we have a fresh entry in the cache, we shouldn't need to go out to the
// network at all to successfully resolve a ppd.
TEST_F(PpdProviderTest, FreshCacheHitNoNetworkTraffic) {
  // Explicitly *not* starting a fake server.
  std::string cached_ppd_contents =
      "These cached contents are different from what's being served";
  auto provider = CreateProvider("en", true);
  Printer::PpdReference ref;
  ref.effective_make_and_model = "printer_a_ref";
  std::string cache_key = PpdProvider::PpdReferenceToCacheKey(ref);
  // Cache exists, and is just created, so should be fresh.
  ppd_cache_->StoreForTesting(PpdProvider::PpdReferenceToCacheKey(ref),
                              cached_ppd_contents, base::TimeDelta());
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

// If we have a stale cache entry and a good network connection, does the cache
// get refreshed during a resolution?
TEST_F(PpdProviderTest, StaleCacheGetsRefreshed) {
  StartFakePpdServer();
  std::string cached_ppd_contents =
      "These cached contents are different from what's being served";
  auto provider = CreateProvider("en", true);
  // printer_ref_a resolves to kCupsFilterPpdContents on the server.
  std::string expected_ppd = kCupsFilterPpdContents;
  Printer::PpdReference ref;
  ref.effective_make_and_model = "printer_a_ref";
  std::string cache_key = PpdProvider::PpdReferenceToCacheKey(ref);
  // Cache exists, and is 6 months old, so really stale.
  ppd_cache_->StoreForTesting(PpdProvider::PpdReferenceToCacheKey(ref),
                              cached_ppd_contents,
                              base::TimeDelta::FromDays(180));
  task_environment_.RunUntilIdle();
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1UL, captured_resolve_ppd_.size());

  // Should get the served results back, not the stale cached ones.
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(captured_resolve_ppd_[0].ppd_contents, expected_ppd);

  // Check that the cache was also updated.
  PpdCache::FindResult captured_find_result;
  // This is just a complicated syntax around the idea "use the Find callback to
  // save the result in captured_find_result.
  ppd_cache_->Find(PpdProvider::PpdReferenceToCacheKey(ref),
                   base::BindOnce(
                       [](PpdCache::FindResult* captured_find_result,
                          const PpdCache::FindResult& find_result) {
                         *captured_find_result = find_result;
                       },
                       &captured_find_result));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_result.success, true);
  EXPECT_EQ(captured_find_result.contents, expected_ppd);
  EXPECT_LT(captured_find_result.age, base::TimeDelta::FromDays(1));
}

// Test that, if we have an old entry in the cache that needs to be refreshed,
// and we fail to contact the server, we still use the cached version.
TEST_F(PpdProviderTest, StaleCacheGetsUsedIfNetworkFails) {
  // Note that we're explicitly *not* starting the Fake ppd server in this test.
  std::string cached_ppd_contents =
      "These cached contents are different from what's being served";
  auto provider = CreateProvider("en", true);
  Printer::PpdReference ref;
  ref.effective_make_and_model = "printer_a_ref";
  std::string cache_key = PpdProvider::PpdReferenceToCacheKey(ref);
  // Cache exists, and is 6 months old, so really stale.
  ppd_cache_->StoreForTesting(PpdProvider::PpdReferenceToCacheKey(ref),
                              cached_ppd_contents,
                              base::TimeDelta::FromDays(180));
  task_environment_.RunUntilIdle();
  provider->ResolvePpd(ref, base::BindOnce(&PpdProviderTest::CaptureResolvePpd,
                                           base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(1UL, captured_resolve_ppd_.size());

  // Should successfully resolve from the cache, even though it's stale.
  EXPECT_EQ(PpdProvider::SUCCESS, captured_resolve_ppd_[0].code);
  EXPECT_EQ(cached_ppd_contents, captured_resolve_ppd_[0].ppd_contents);

  // Check that the cache is *not* updated; it should remain stale.
  PpdCache::FindResult captured_find_result;
  // This is just a complicated syntax around the idea "use the Find callback to
  // save the result in captured_find_result.
  ppd_cache_->Find(PpdProvider::PpdReferenceToCacheKey(ref),
                   base::BindOnce(
                       [](PpdCache::FindResult* captured_find_result,
                          const PpdCache::FindResult& find_result) {
                         *captured_find_result = find_result;
                       },
                       &captured_find_result));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_result.success, true);
  EXPECT_EQ(captured_find_result.contents, cached_ppd_contents);
  EXPECT_GT(captured_find_result.age, base::TimeDelta::FromDays(179));
}

// For user-provided ppds, we should always use the latest version on
// disk if it still exists there.
TEST_F(PpdProviderTest, UserPpdAlwaysRefreshedIfAvailable) {
  base::ScopedTempDir temp_dir;
  StartFakePpdServer();
  std::string cached_ppd_contents = "Cached Ppd Contents";
  std::string disk_ppd_contents = "Updated Ppd Contents";
  auto provider = CreateProvider("en", true);
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
  StartFakePpdServer();
  auto provider = CreateProvider("en", false);

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

}  // namespace
}  // namespace chromeos
