// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_metadata_manager.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "chromeos/printing/fake_printer_config_cache.h"
#include "chromeos/printing/ppd_metadata_matchers.h"
#include "chromeos/printing/ppd_metadata_parser.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_config_cache.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

// Arbitrarily chosen TimeDelta used in test cases that are not
// time-senstive.
constexpr base::TimeDelta kArbitraryTimeDelta = base::Seconds(30LL);

// Arbitrarily malformed JSON used to exercise code paths in which
// parsing fails.
constexpr std::string_view kInvalidJson = "blah blah invalid JSON";

// Caller may bind a default-constructed base::RepeatingClosure to
// any Catch*() method, indicating that they don't want anything run.
class PpdMetadataManagerTest : public ::testing::Test {
 public:

  // Callback method appropriate for passing to
  // PpdMetadataManager::GetManufacturers().
  void CatchGetManufacturers(base::RepeatingClosure quit_closure,
                             PpdProvider::CallbackResultCode code,
                             const std::vector<std::string>& manufacturers) {
    results_.get_manufacturers_code = code;
    results_.manufacturers = manufacturers;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

  // Callback method appropriate for passing to
  // PpdMetadataManager::GetPrinters().
  void CatchGetPrinters(base::RepeatingClosure quit_closure,
                        bool succeeded,
                        const ParsedPrinters& printers) {
    results_.get_printers_succeeded = succeeded;
    results_.printers = printers;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

  // Callback method appropriate for passing to
  // PpdMetadataManager::FindAllEmmsAvailableInIndex().
  void CatchFindAllEmmsAvailableInIndex(
      base::RepeatingClosure quit_closure,
      const base::flat_map<std::string, ParsedIndexValues>& values) {
    results_.available_effective_make_and_model_strings = values;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

  // Callback method appropriate for passing to
  // PpdMetadataManager::FindDeviceInUsbIndex().
  void CatchFindDeviceInUsbIndex(base::RepeatingClosure quit_closure,
                                 const std::string& value) {
    results_.effective_make_and_model_string_from_usb_index = value;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

  // Callback method appropriate for passing to
  // PpdMetadataManager::GetUsbManufacturerName().
  void CatchGetUsbManufacturerName(base::RepeatingClosure quit_closure,
                                   const std::string& value) {
    results_.usb_manufacturer_name = value;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

  // Callback method appropriate for passing to
  // PpdMetadataManager::SplitMakeAndModel().
  void CatchSplitMakeAndModel(base::RepeatingClosure quit_closure,
                              PpdProvider::CallbackResultCode code,
                              const std::string& make,
                              const std::string& model) {
    results_.split_make_and_model_code = code;
    results_.split_make = make;
    results_.split_model = model;
    if (quit_closure) {
      quit_closure.Run();
    }
  }

 protected:
  // Convenience container that organizes all callback results.
  struct CallbackLandingArea {
    CallbackLandingArea() : get_printers_succeeded(false) {}
    ~CallbackLandingArea() = default;

    // Landing area for PpdMetadataManager::GetManufacturers().
    PpdProvider::CallbackResultCode get_manufacturers_code;
    std::vector<std::string> manufacturers;

    // Landing area for PpdMetadataManager::GetPrinters().
    bool get_printers_succeeded;
    ParsedPrinters printers;

    // Landing area for
    // PpdMetadataManager::FindAllEmmsAvailableInIndex().
    base::flat_map<std::string, ParsedIndexValues>
        available_effective_make_and_model_strings;

    // Landing area for
    // PpdMetadataManager::FindDeviceInUsbIndex().
    std::string effective_make_and_model_string_from_usb_index;

    // Landing area for
    // PpdMetadataManager::GetUsbManufacturerName().
    std::string usb_manufacturer_name;

    // Landing area for PpdMetadataManager::SplitMakeAndModel().
    PpdProvider::CallbackResultCode split_make_and_model_code;
    std::string split_make;
    std::string split_model;
  };

  PpdMetadataManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        manager_(PpdMetadataManager::Create(
            PpdIndexChannel::kProduction,
            &clock_,
            std::make_unique<FakePrinterConfigCache>())) {}

  // Borrows and returns a pointer to the config cache owned by the
  // |manager_|.
  //
  // Useful for adjusting availability of (fake) network resources.
  FakePrinterConfigCache* GetFakeCache() {
    return reinterpret_cast<FakePrinterConfigCache*>(
        manager_->GetPrinterConfigCacheForTesting());
  }

  // Holder for all callback results.
  CallbackLandingArea results_;

  // Environment for task schedulers.
  base::test::TaskEnvironment task_environment_;

  // Controlled clock that dispenses times of Fetch().
  base::SimpleTestClock clock_;

  // Class under test.
  std::unique_ptr<PpdMetadataManager> manager_;
};

// Verifies that the manager can fetch, parse, and return a list of
// manufacturers from the Chrome OS Printing serving root.
TEST_F(PpdMetadataManagerTest, CanGetManufacturers) {
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/manufacturers-en.json",
      R"({ "filesMap": {
        "It": "Never_Ends-en.json",
        "You Are": "Always-en.json",
        "Playing": "Yellow_Car-en.json"
      } })");

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetManufacturers,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetManufacturers,
                             base::Unretained(manager_.get()),
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(call));
  loop.Run();

  ASSERT_EQ(results_.get_manufacturers_code,
            PpdProvider::CallbackResultCode::SUCCESS);

  // PpdProvider::ResolveManufacturersCallback specifies that the list
  // shall be sorted.
  EXPECT_THAT(results_.manufacturers,
              ElementsAre(StrEq("It"), StrEq("Playing"), StrEq("You Are")));
}

// Verifies that the manager fails the ResolveManufacturersCallback
// when it fails to fetch the manufacturers metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetManufacturersOnFetchFailure) {
  // In this test case, we do _not_ populate the fake config cache with
  // the appropriate metadata, causing the fetch to fail.

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetManufacturers,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetManufacturers,
                             base::Unretained(manager_.get()),
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(call));
  loop.Run();

  ASSERT_EQ(results_.get_manufacturers_code,
            PpdProvider::CallbackResultCode::SERVER_ERROR);
}

// Verifies that the manager fails the ResolveManufacturersCallback
// when it fails to parse the manufacturers metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetManufacturersOnParseFailure) {
  // Known interaction: the manager will fetch manufacturers metadata.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/manufacturers-en.json", kInvalidJson);

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetManufacturers,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetManufacturers,
                             base::Unretained(manager_.get()),
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(call));
  loop.Run();

  ASSERT_EQ(results_.get_manufacturers_code,
            PpdProvider::CallbackResultCode::INTERNAL_ERROR);
}

// Verifies that the manager fetches manufacturers metadata anew when
// caller asks it for metadata fresher than what it has cached.
TEST_F(PpdMetadataManagerTest, CanGetManufacturersTimeSensitive) {
  // Known interaction: the manager will fetch manufacturers metadata.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/manufacturers-en.json",
      R"({ "filesMap": {
        "It": "Never_Ends-en.json",
        "You Are": "Always-en.json",
        "Playing": "Yellow_Car-en.json"
      } })");

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetManufacturers,
                                 base::Unretained(this), loop.QuitClosure());

  // t = 0s: caller requests a list of manufacturers from |manager_|,
  // using metadata parsed within the last thirty seconds. |manager_|
  // performs a live fetch for the manufacturers metadata.
  manager_->GetManufacturers(base::Seconds(30), std::move(callback));
  loop.Run();

  ASSERT_EQ(results_.get_manufacturers_code,
            PpdProvider::CallbackResultCode::SUCCESS);

  // PpdProvider::ResolveManufacturersCallback specifies that the list
  // shall be sorted.
  EXPECT_THAT(results_.manufacturers,
              ElementsAre(StrEq("It"), StrEq("Playing"), StrEq("You Are")));

  // Mutates the test data, ensuring that if the metadata manager
  // attempts a live fetch from the PrinterConfigCache, it will get
  // different data in response.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/manufacturers-en.json",
      R"({ "filesMap": {
        "It": "Never_Ends-en.json",
        "You Are": "Always-en.json"
      } })");

  // Jams the result code to something bad to require that the
  // |manager_| positively answer us.
  results_.get_manufacturers_code =
      PpdProvider::CallbackResultCode::INTERNAL_ERROR;

  base::RunLoop second_loop;
  auto second_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetManufacturers,
                     base::Unretained(this), second_loop.QuitClosure());

  // t = 0s: caller requests a list of manufacturers from |manager_|.
  // |manager_| responds using the cached metadata without performing
  // a live fetch.
  manager_->GetManufacturers(base::Seconds(30), std::move(second_callback));
  second_loop.Run();

  // We assert that the results are unchanged from before.
  ASSERT_EQ(results_.get_manufacturers_code,
            PpdProvider::CallbackResultCode::SUCCESS);
  EXPECT_THAT(results_.manufacturers,
              ElementsAre(StrEq("It"), StrEq("Playing"), StrEq("You Are")));

  // t = 31s
  clock_.Advance(base::Seconds(31));

  // Jams the result code to something bad to require that the
  // |manager_| positively answer us.
  results_.get_manufacturers_code =
      PpdProvider::CallbackResultCode::INTERNAL_ERROR;

  base::RunLoop third_loop;
  auto third_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetManufacturers,
                     base::Unretained(this), third_loop.QuitClosure());

  // t = 31s: caller requests a list of manufacturers from |manager_|.
  // |manager_| does not have sufficiently fresh metadata, so it
  // performs a live fetch.
  manager_->GetManufacturers(base::Seconds(30), std::move(third_callback));
  third_loop.Run();

  // We assert that the results have changed.
  ASSERT_EQ(results_.get_manufacturers_code,
            PpdProvider::CallbackResultCode::SUCCESS);
  EXPECT_THAT(results_.manufacturers,
              ElementsAre(StrEq("It"), StrEq("You Are")));
}

// Verifies that the manager can fetch, parse, and return a map of
// printers metadata from the Chrome OS Printing serving root.
TEST_F(PpdMetadataManagerTest, CanGetPrinters) {
  // Bypasses prerequisite call to PpdMetadataManager::GetManufacturers().
  ASSERT_TRUE(manager_->SetManufacturersForTesting(R"(
  {
    "filesMap": {
      "Manufacturer A": "Manufacturer_A-en.json",
      "Manufacturer B": "Manufacturer_B-en.json"
    }
  }
  )"));

  // Known interaction: the manager will fetch printers metadata named
  // by the manufacturers map above.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/Manufacturer_A-en.json", R"(
      {
        "printers": [ {
          "emm": "some emm a",
          "name": "Some Printer A"
        }, {
          "emm": "some emm b",
          "name": "Some Printer B"
        } ]
      }
  )");

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetPrinters,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetPrinters,
                             base::Unretained(manager_.get()), "Manufacturer A",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(call));
  loop.Run();

  ASSERT_TRUE(results_.get_printers_succeeded);
  EXPECT_THAT(
      results_.printers,
      UnorderedElementsAre(ParsedPrinterLike("Some Printer A", "some emm a"),
                           ParsedPrinterLike("Some Printer B", "some emm b")));
}

// Verifies that the manager fails the GetPrintersCallback when it fails
// to fetch the printers metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetPrintersOnFetchFailure) {
  // Bypasses prerequisite call to PpdMetadataManager::GetManufacturers().
  ASSERT_TRUE(manager_->SetManufacturersForTesting(R"(
  {
    "filesMap": {
      "Manufacturer A": "Manufacturer_A-en.json",
      "Manufacturer B": "Manufacturer_B-en.json"
    }
  }
  )"));

  // This test is set up like the CanGetPrinters test case above, but we
  // elect _not_ to provide a response for any printers metadata,
  // causing the fetch to fail.
  //
  // We set the result value to the opposite of what's expected to
  // observe the change.
  results_.get_printers_succeeded = true;

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetPrinters,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetPrinters,
                             base::Unretained(manager_.get()), "Manufacturer A",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(call));
  loop.Run();

  EXPECT_FALSE(results_.get_printers_succeeded);
}

// Verifies that the manager fails the GetPrintersCallback when it fails
// to parse the printers metadata.
TEST_F(PpdMetadataManagerTest, FailsToGetPrintersOnParseFailure) {
  // Bypasses prerequisite call to PpdMetadataManager::GetManufacturers().
  ASSERT_TRUE(manager_->SetManufacturersForTesting(R"(
  {
    "filesMap": {
      "Manufacturer A": "Manufacturer_A-en.json",
      "Manufacturer B": "Manufacturer_B-en.json"
    }
  }
  )"));

  // This test is set up like the CanGetPrinters test case above, but we
  // elect to provide a malformed JSON response for the printers
  // metadata, which will cause the manager to fail parsing.
  //
  // Known interaction: the manager will fetch the printers metadata
  // named by the map above.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/Manufacturer_A-en.json", kInvalidJson);

  // We set the result value to the opposite of what's expected to
  // observe the change.
  results_.get_printers_succeeded = true;

  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetPrinters,
                                 base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::GetPrinters,
                             base::Unretained(manager_.get()), "Manufacturer A",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(call));
  loop.Run();

  EXPECT_FALSE(results_.get_printers_succeeded);
}

// Verifies that the manager fetches printers metadata anew when caller
// asks it for metadata fresher than what it has cached.
TEST_F(PpdMetadataManagerTest, CanGetPrintersTimeSensitive) {
  // Bypasses prerequisite call to PpdMetadataManager::GetManufacturers().
  ASSERT_TRUE(manager_->SetManufacturersForTesting(R"(
  {
    "filesMap": {
      "Manufacturer A": "Manufacturer_A-en.json",
      "Manufacturer B": "Manufacturer_B-en.json"
    }
  }
  )"));

  // Known interaction: the manager will fetch printers metadata named
  // by the manufacturers map above.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/Manufacturer_A-en.json", R"(
      {
        "printers": [ {
          "emm": "some emm a",
          "name": "Some Printer A"
        }, {
          "emm": "some emm b",
          "name": "Some Printer B"
        } ]
      }
  )");

  // t = 0s: caller requests the printers for "Manufacturer A."
  // |manager_| parses and caches the metadata successfully.
  base::RunLoop loop;
  auto callback = base::BindOnce(&PpdMetadataManagerTest::CatchGetPrinters,
                                 base::Unretained(this), loop.QuitClosure());
  manager_->GetPrinters("Manufacturer A", base::Seconds(30),
                        std::move(callback));
  loop.Run();

  ASSERT_TRUE(results_.get_printers_succeeded);
  EXPECT_THAT(
      results_.printers,
      UnorderedElementsAre(ParsedPrinterLike("Some Printer A", "some emm a"),
                           ParsedPrinterLike("Some Printer B", "some emm b")));

  // We change the data served by the PrinterConfigCache.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/Manufacturer_A-en.json", R"(
      {
        "printers": [ {
          "emm": "some emm c",
          "name": "Some Printer C"
        }, {
          "emm": "some emm d",
          "name": "Some Printer D"
        } ]
      }
  )");

  // Jams the results to some bad value, requiring that the manager
  // answer us positively.
  results_.get_printers_succeeded = false;

  base::RunLoop second_loop;
  auto second_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetPrinters,
                     base::Unretained(this), second_loop.QuitClosure());

  // t = 0s: caller requests the printers for "Manufacturer A."
  // |manager_| re-uses the cached metadata.
  manager_->GetPrinters("Manufacturer A", base::Seconds(30),
                        std::move(second_callback));
  second_loop.Run();

  // We assert that the results are unchanged from before.
  ASSERT_TRUE(results_.get_printers_succeeded);
  EXPECT_THAT(
      results_.printers,
      UnorderedElementsAre(ParsedPrinterLike("Some Printer A", "some emm a"),
                           ParsedPrinterLike("Some Printer B", "some emm b")));

  // t = 31s
  clock_.Advance(base::Seconds(31));

  // Jams the results to some bad value, requiring that the manager
  // answer us positively.
  results_.get_printers_succeeded = false;

  // t = 31s: caller requests the printers for "Manufacturer A."
  // |manager_| does not have sufficiently fresh metadata, so it
  // performs a live fetch.
  base::RunLoop third_loop;
  auto third_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetPrinters,
                     base::Unretained(this), third_loop.QuitClosure());
  manager_->GetPrinters("Manufacturer A", base::Seconds(30),
                        std::move(third_callback));
  third_loop.Run();

  // We assert that the results have changed.
  ASSERT_TRUE(results_.get_printers_succeeded);
  EXPECT_THAT(
      results_.printers,
      UnorderedElementsAre(ParsedPrinterLike("Some Printer C", "some emm c"),
                           ParsedPrinterLike("Some Printer D", "some emm d")));
}

// Verifies that the manager can find all effective-make-and-model
// strings in forward index metadata.
TEST_F(PpdMetadataManagerTest, CanFindAllAvailableEmmsInIndex) {
  // Known interaction: the manager will fetch forward index metadata
  // numbered 14, 15, and 16.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-14.json", R"(
  {
    "ppdIndex": {
      "some printer a": {
        "ppdMetadata": [ {
          "name": "some-ppd-basename-a.ppd.gz"
        } ]
      }
    }
  })");
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-15.json", R"(
  {
    "ppdIndex": {
      "some printer b": {
        "ppdMetadata": [ {
          "name": "some-ppd-basename-b.ppd.gz"
        } ]
      }
    }
  })");
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-16.json", R"(
  {
    "ppdIndex": {
      "some printer c": {
        "ppdMetadata": [ {
          "name": "some-ppd-basename-c.ppd.gz"
        } ]
      }
    }
  })");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindAllEmmsAvailableInIndex,
                     base::Unretained(this), loop.QuitClosure());
  manager_->FindAllEmmsAvailableInIndex(
      {"some printer a", "some printer b", "some printer c"},
      kArbitraryTimeDelta, std::move(callback));
  loop.Run();

  EXPECT_THAT(results_.available_effective_make_and_model_strings,
              UnorderedElementsAre(
                  ParsedIndexEntryLike(
                      "some printer a",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-a.ppd.gz"))),
                  ParsedIndexEntryLike(
                      "some printer b",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-b.ppd.gz"))),
                  ParsedIndexEntryLike(
                      "some printer c",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-c.ppd.gz")))));
}

// Verifies that the manager invokes the
// FindAllAvailableEmmsInIndexCallback with a partially filled argument
// if it fails to fetch some of the necessary metadata.
TEST_F(PpdMetadataManagerTest,
       CanPartiallyFindAllAvailableEmmsInIndexWithFetchFailure) {
  // Known interaction: the manager will fetch forward index metadata
  // numbered 14, 15, and 16.
  //
  // We deliberately omit forward index metadata no. 14 to simulate a
  // fetch failure.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-15.json", R"(
  {
    "ppdIndex": {
      "some printer b": {
        "ppdMetadata": [ {
          "name": "some-ppd-basename-b.ppd.gz"
        } ]
      }
    }
  })");
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-16.json", R"(
  {
    "ppdIndex": {
      "some printer c": {
        "ppdMetadata": [ {
          "name": "some-ppd-basename-c.ppd.gz"
        } ]
      }
    }
  })");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindAllEmmsAvailableInIndex,
                     base::Unretained(this), loop.QuitClosure());
  manager_->FindAllEmmsAvailableInIndex(
      {"some printer a", "some printer b", "some printer c"},
      kArbitraryTimeDelta, std::move(callback));
  loop.Run();

  // The manager was unable to get the forward index metadata that would
  // contain information for effective-make-and-model string
  // "some printer a," but the other results should avail themselves.
  EXPECT_THAT(results_.available_effective_make_and_model_strings,
              UnorderedElementsAre(
                  ParsedIndexEntryLike(
                      "some printer b",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-b.ppd.gz"))),
                  ParsedIndexEntryLike(
                      "some printer c",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-c.ppd.gz")))));
}

// Verifies that the manager invokes the
// FindAllAvailableEmmsInIndexCallback with a partially filled argument
// if it fails to parse some of the necessary metadata.
TEST_F(PpdMetadataManagerTest,
       CanPartiallyFindAllAvailableEmmsInIndexWithParseFailure) {
  // Known interaction: the manager will fetch forward index metadata
  // numbered 14, 15, and 16.
  //
  // We deliberately serve malformed JSON that will fail to parse for
  // indices nos. 14 and 15 to exercise the manager's handling of parse
  // failures.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-14.json",
                                             kInvalidJson);
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-15.json",
                                             kInvalidJson);
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-16.json", R"(
  {
    "ppdIndex": {
      "some printer c": {
        "ppdMetadata": [ {
          "name": "some-ppd-basename-c.ppd.gz"
        } ]
      }
    }
  })");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindAllEmmsAvailableInIndex,
                     base::Unretained(this), loop.QuitClosure());
  manager_->FindAllEmmsAvailableInIndex(
      {"some printer a", "some printer b", "some printer c"},
      kArbitraryTimeDelta, std::move(callback));
  loop.Run();

  // The manager was unable to parse the forward index metadata that would
  // contain information for effective-make-and-model strings
  // "some printer a" and for "some printer b," but the last string
  // "some printer c" should avail itself.
  EXPECT_THAT(
      results_.available_effective_make_and_model_strings,
      UnorderedElementsAre(ParsedIndexEntryLike(
          "some printer c", UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                                "some-ppd-basename-c.ppd.gz")))));
}

// Verifies that the manager fetches forward index metadata anew when
// the caller asks it for metadata fresher than what it has cached.
TEST_F(PpdMetadataManagerTest, CanFindAllAvailableEmmsInIndexTimeSensitive) {
  // Known interaction: the manager will fetch forward index metadata
  // numbered 14, 15, and 16.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-14.json", R"(
  {
    "ppdIndex": {
      "some printer a": {
        "ppdMetadata": [ {
          "name": "some-ppd-basename-a.ppd.gz"
        } ]
      }
    }
  })");
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-15.json", R"(
  {
    "ppdIndex": {
      "some printer b": {
        "ppdMetadata": [ {
          "name": "some-ppd-basename-b.ppd.gz"
        } ]
      }
    }
  })");
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/index-16.json", R"(
  {
    "ppdIndex": {
      "some printer c": {
        "ppdMetadata": [ {
          "name": "some-ppd-basename-c.ppd.gz"
        } ]
      }
    }
  })");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindAllEmmsAvailableInIndex,
                     base::Unretained(this), loop.QuitClosure());

  // t = 0s: caller requests the |manager_| to search forward index
  // metadata for three effective-make-and-model strings. |manager_|
  // fetches the appropriate forward index metadata (nos. 14, 15, and
  // 16) and caches the result.
  manager_->FindAllEmmsAvailableInIndex(
      {"some printer a", "some printer b", "some printer c"}, base::Seconds(30),
      std::move(callback));
  loop.Run();

  EXPECT_THAT(results_.available_effective_make_and_model_strings,
              UnorderedElementsAre(
                  ParsedIndexEntryLike(
                      "some printer a",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-a.ppd.gz"))),
                  ParsedIndexEntryLike(
                      "some printer b",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-b.ppd.gz"))),
                  ParsedIndexEntryLike(
                      "some printer c",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-c.ppd.gz")))));

  // We drop forward index metadata nos. 14 and 15 now. If the
  // |manager_| attempts to fetch these again, it will fail to do so.
  GetFakeCache()->Drop("metadata_v3/index-14.json");
  GetFakeCache()->Drop("metadata_v3/index-15.json");

  // Resets the results to require that the |manager_| answer us
  // positively.
  results_.available_effective_make_and_model_strings = {};

  base::RunLoop second_loop;
  auto second_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindAllEmmsAvailableInIndex,
                     base::Unretained(this), second_loop.QuitClosure());

  // t = 0s: caller requests the |manager_| to search forward index
  // metadata for the same three effective-make-and-model strings.
  // Caller requests this using metadata fetched within the last thirty
  // seconds, so |manager_| does not perform a live fetch of the data.
  manager_->FindAllEmmsAvailableInIndex(
      {"some printer a", "some printer b", "some printer c"}, base::Seconds(30),
      std::move(second_callback));
  second_loop.Run();

  // We expect the results to be unchanged.
  EXPECT_THAT(results_.available_effective_make_and_model_strings,
              UnorderedElementsAre(
                  ParsedIndexEntryLike(
                      "some printer a",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-a.ppd.gz"))),
                  ParsedIndexEntryLike(
                      "some printer b",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-b.ppd.gz"))),
                  ParsedIndexEntryLike(
                      "some printer c",
                      UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                          "some-ppd-basename-c.ppd.gz")))));

  // t = 31s
  clock_.Advance(base::Seconds(31));

  base::RunLoop third_loop;
  auto third_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindAllEmmsAvailableInIndex,
                     base::Unretained(this), third_loop.QuitClosure());

  // t = 31s: caller requests the |manager_| to search forward index
  // metadata for the same three effective-make-and-model strings.
  // Caller requests this using metadata fetched within the last thirty
  // thirds; |manager_| sees that its cached metadata is too old, and
  // so performs a live fetch.
  //
  // Since we previously blocked forward index metadata nos. 14 and 15
  // from being served, the |manager_| will fail to fetch these.
  manager_->FindAllEmmsAvailableInIndex(
      {"some printer a", "some printer b", "some printer c"}, base::Seconds(30),
      std::move(third_callback));
  third_loop.Run();

  // We expect the results to have changed.
  EXPECT_THAT(
      results_.available_effective_make_and_model_strings,
      UnorderedElementsAre(ParsedIndexEntryLike(
          "some printer c", UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                                "some-ppd-basename-c.ppd.gz")))));
}

// Verifies that the manager can find a USB device by fetching and
// parsing USB index metadata.
TEST_F(PpdMetadataManagerTest, CanFindDeviceInUsbIndex) {
  // Known interaction: hex(1138) == 0x472. To fetch USB index metadata
  // for a manufacturer with vendor ID 1138, the manager will fetch
  // the metadata with the following name.
  //
  // This USB index describes one product for vendor ID 1138; its
  // product ID is 13.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/usb-0472.json", R"({
  "usbIndex": {
    "13": {
      "effectiveMakeAndModel": "some printer a"
    }
  }
})");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindDeviceInUsbIndex,
                     base::Unretained(this), loop.QuitClosure());
  manager_->FindDeviceInUsbIndex(1138, 13, kArbitraryTimeDelta,
                                 std::move(callback));
  loop.Run();

  EXPECT_THAT(results_.effective_make_and_model_string_from_usb_index,
              StrEq("some printer a"));
}

// Verifies that the manager invokes the FindDeviceInUsbIndexCallback
// with an empty argument if it fails to fetch the appropriate USB
// index.
TEST_F(PpdMetadataManagerTest, FailsToFindDeviceInUsbIndexOnFetchFailure) {
  // Known interaction: hex(1138) == 0x472. To fetch USB index metadata
  // for a manufacturer with vendor ID 1138, the manager will fetch
  // the metadata with the following name.
  //
  // We populate nothing in the fake serving root, so any fetch request
  // from the manager will fail.

  // Jams the landing area to have a non-empty string. We expect the
  // callback to fire with an empty string, which should empty this.
  results_.effective_make_and_model_string_from_usb_index =
      "non-empty string that will fail this test if it persists";

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindDeviceInUsbIndex,
                     base::Unretained(this), loop.QuitClosure());
  manager_->FindDeviceInUsbIndex(1138, 13, kArbitraryTimeDelta,
                                 std::move(callback));
  loop.Run();

  EXPECT_TRUE(results_.effective_make_and_model_string_from_usb_index.empty());
}

// Verifies that the manager invokes the FindDeviceInUsbIndexCallback
// with an empty argument if it fails to parse the appropriate USB
// index.
TEST_F(PpdMetadataManagerTest, FailsToFindDeviceInUsbIndexOnParseFailure) {
  // Known interaction: hex(1138) == 0x472. To fetch USB index metadata
  // for a manufacturer with vendor ID 1138, the manager will fetch
  // the metadata with the following name.
  //
  // We populate the fake serving root with invalid JSON for the USB
  // index metadata that the manager will fetch and fail to parse.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/usb-0472.json",
                                             kInvalidJson);

  // Jams the landing area to have a non-empty string. We expect the
  // callback to fire with an empty string, which should empty this.
  results_.effective_make_and_model_string_from_usb_index =
      "non-empty string that will fail this test if it persists";

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindDeviceInUsbIndex,
                     base::Unretained(this), loop.QuitClosure());
  manager_->FindDeviceInUsbIndex(1138, 13, kArbitraryTimeDelta,
                                 std::move(callback));
  loop.Run();

  EXPECT_TRUE(results_.effective_make_and_model_string_from_usb_index.empty());
}

// Verifies that the manager fetches USB index metadata anew when caller
// asks it for metadata fresher than what it has cached.
TEST_F(PpdMetadataManagerTest, CanFindDeviceInUsbIndexTimeSensitive) {
  // Known interaction: hex(1138) == 0x472. To fetch USB index metadata
  // for a manufacturer with vendor ID 1138, the manager will fetch
  // the metadata with the following name.
  //
  // This USB index describes one product for vendor ID 1138; its
  // product ID is 13.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/usb-0472.json", R"({
  "usbIndex": {
    "13": {
      "effectiveMakeAndModel": "some printer a"
    }
  }
})");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindDeviceInUsbIndex,
                     base::Unretained(this), loop.QuitClosure());

  // t = 0s: caller requests |manager_| to name a device with vendor ID
  // 1138 and product ID 13. |manager_| fetches, parses, and caches
  // the appropriate USB index metadata.
  manager_->FindDeviceInUsbIndex(1138, 13, kArbitraryTimeDelta,
                                 std::move(callback));
  loop.Run();

  EXPECT_THAT(results_.effective_make_and_model_string_from_usb_index,
              StrEq("some printer a"));

  // Sets the serving root to mutate the served USB index metadata; the
  // device with product ID 13 now has the effective-make-and-model
  // string "some printer b." If the |manager_| fetches this metadata
  // anew, then it will observe the changed effective-make-and-model
  // string.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/usb-0472.json", R"({
  "usbIndex": {
    "13": {
      "effectiveMakeAndModel": "some printer b"
    }
  }
})");

  // Jams the landing area to hold a test-failing string. We expect
  // the successful callback to overwrite this.
  results_.effective_make_and_model_string_from_usb_index =
      "non-empty string that will fail this test if it persists";

  base::RunLoop second_loop;
  auto second_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindDeviceInUsbIndex,
                     base::Unretained(this), second_loop.QuitClosure());

  // t = 0s: caller requests |manager_| to name a device with vendor ID
  // 1138 and product ID 13. It asks that |manager_| do so with metadata
  // parsed within the last 30 seconds. |manager_| responds with the
  // cached USB index metadata without incurring a live fetch.
  manager_->FindDeviceInUsbIndex(1138, 13, base::Seconds(30),
                                 std::move(second_callback));
  second_loop.Run();

  // The manager will have responded with the cached
  // effective-make-and-model string "some printer a."
  EXPECT_THAT(results_.effective_make_and_model_string_from_usb_index,
              StrEq("some printer a"));

  // t = 31s
  clock_.Advance(base::Seconds(31));

  // Jams the landing area to hold a test-failing string. We expect
  // the successful callback to overwrite this.
  results_.effective_make_and_model_string_from_usb_index =
      "non-empty string that will fail this test if it persists";

  base::RunLoop third_loop;
  auto third_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchFindDeviceInUsbIndex,
                     base::Unretained(this), third_loop.QuitClosure());

  // t = 31s: caller requests |manager_| to name a device with vendor ID
  // 1138 and product ID 13. It asks that |manager_| do so with metadata
  // parsed within the last 30 seconds. |manager_| sees that the cached
  // USB index metadata is too stale, and so incurs a live fetch. The
  // fetch exposes the changed metadata.
  manager_->FindDeviceInUsbIndex(1138, 13, base::Seconds(30),
                                 std::move(third_callback));
  third_loop.Run();

  // The manager will have responded with the new and changed
  // effective-make-and-model string "some printer b."
  EXPECT_THAT(results_.effective_make_and_model_string_from_usb_index,
              StrEq("some printer b"));
}

// Verifies that the manager can determine a USB manufacturer name
// by fetching and searching the USB vendor ID map.
TEST_F(PpdMetadataManagerTest, CanGetUsbManufacturerName) {
  // Known interaction: |manager_| shall fetch the USB vendor ID map.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/usb_vendor_ids.json",
                                             R"({
  "entries": [ {
    "vendorId": 1138,
    "vendorName": "Some Vendor Name"
  } ]
})");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetUsbManufacturerName,
                     base::Unretained(this), loop.QuitClosure());

  manager_->GetUsbManufacturerName(1138, kArbitraryTimeDelta,
                                   std::move(callback));
  loop.Run();

  EXPECT_THAT(results_.usb_manufacturer_name, StrEq("Some Vendor Name"));
}

// Verifies that the manager invokes the GetUsbManufacturerNameCallback
// with an empty argument if it fails to fetch the USB vendor ID map.
TEST_F(PpdMetadataManagerTest, FailsToGetUsbManufacturerNameOnFetchFailure) {
  // Known interaction: |manager_| shall fetch the USB vendor ID map.
  //
  // We deliberately don't set any fetch response in the fake serving
  // root; |manager_| will fail to fetch the USB vendor ID map. However,
  // we do jam the landing area with a sentinel value to ensure that the
  // callback does fire with an empty string.
  results_.usb_manufacturer_name =
      "non-empty string that will fail this test if it persists";

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetUsbManufacturerName,
                     base::Unretained(this), loop.QuitClosure());

  manager_->GetUsbManufacturerName(1138, kArbitraryTimeDelta,
                                   std::move(callback));
  loop.Run();

  EXPECT_TRUE(results_.usb_manufacturer_name.empty());
}

// Verifies that the manager invokes the GetUsbManufacturerNameCallback
// with an empty argument if it fails to parse the USB vendor ID map.
TEST_F(PpdMetadataManagerTest, FailsToGetUsbManufacturerNameOnParseFailure) {
  // Known interaction: |manager_| shall fetch the USB vendor ID map.
  //
  // We deliberately set a malformed response in the serving root;
  // |manager_| will fetch the USB vendor ID map successfully, but will
  // fail to parse it.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/usb_vendor_ids.json",
                                             kInvalidJson);

  // We also jam the landing area with a sentinel value to ensure that
  // the callback fires with an empty string.
  results_.usb_manufacturer_name =
      "non-empty string that will fail this test if it persists";

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetUsbManufacturerName,
                     base::Unretained(this), loop.QuitClosure());

  manager_->GetUsbManufacturerName(1138, kArbitraryTimeDelta,
                                   std::move(callback));
  loop.Run();

  EXPECT_TRUE(results_.usb_manufacturer_name.empty());
}

// Verifies that the manager fetches the USB vendor ID map anew if the
// caller calls GetUsbManufacturerName() asking for metadata fresher
// than what it has cached.
TEST_F(PpdMetadataManagerTest, CanGetUsbManufacturerNameTimeSensitive) {
  // Known interaction: |manager_| shall fetch the USB vendor ID map.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/usb_vendor_ids.json",
                                             R"({
  "entries": [ {
    "vendorId": 1138,
    "vendorName": "Vendor One One Three Eight"
  } ]
})");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetUsbManufacturerName,
                     base::Unretained(this), loop.QuitClosure());

  // t = 0s: caller requests the name of a USB manufacturer whose vendor
  // ID is 1138. |manager_| fetches, parses, and caches the USB vendor
  // ID map, responding with the name.
  manager_->GetUsbManufacturerName(1138, base::Seconds(30),
                                   std::move(callback));
  loop.Run();

  EXPECT_THAT(results_.usb_manufacturer_name,
              StrEq("Vendor One One Three Eight"));

  // Mutates the USB vendor ID map served by the fake serving root.
  // If the |manager_| fetches it now, it will see a changed name for
  // the USB manufacturer with vendor ID 1138.
  GetFakeCache()->SetFetchResponseForTesting("metadata_v3/usb_vendor_ids.json",
                                             R"({
  "entries": [ {
    "vendorId": 1138,
    "vendorName": "One One Three Eight LLC"
  } ]
})");

  base::RunLoop second_loop;
  auto second_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetUsbManufacturerName,
                     base::Unretained(this), second_loop.QuitClosure());

  // t = 0s: caller requests the name of a USB manufacturer whose vendor
  // ID is 1138. |manager_| responds with the previously fetched
  // metadata.
  manager_->GetUsbManufacturerName(1138, base::Seconds(30),
                                   std::move(second_callback));
  second_loop.Run();

  // Since |manager_| has not fetched the mutated USB vendor ID map,
  // the results are unchanged from before.
  EXPECT_THAT(results_.usb_manufacturer_name,
              StrEq("Vendor One One Three Eight"));

  // t = 31s
  clock_.Advance(base::Seconds(31));

  base::RunLoop third_loop;
  auto third_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchGetUsbManufacturerName,
                     base::Unretained(this), third_loop.QuitClosure());

  // t = 31s: caller requests the name of a USB manufacturer whose
  // vendor ID is 1138. |manager_| notices that its cached metadata is
  // too stale and performs a live fetch, receiving the mutated
  // USB vendor ID map.
  manager_->GetUsbManufacturerName(1138, base::Seconds(30),
                                   std::move(third_callback));
  third_loop.Run();

  // Since |manager_| has not fetched the mutated USB vendor ID map,
  // the results are unchanged from before.
  EXPECT_THAT(results_.usb_manufacturer_name, StrEq("One One Three Eight LLC"));
}

// Verifies that the manager can split an effective-make-and-model
// string into its constituent parts (make and model).
TEST_F(PpdMetadataManagerTest, CanSplitMakeAndModel) {
  // Known interaction: asking the manager to split the string
  // "Hello there!" will cause it to fetch the reverse index metadata
  // with shard number 2.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/reverse_index-en-02.json", R"(
      {
        "reverseIndex": {
          "Hello there!": {
            "manufacturer": "General",
            "model": "Kenobi"
          }
        }
      }
  )");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchSplitMakeAndModel,
                     base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::SplitMakeAndModel,
                             base::Unretained(manager_.get()), "Hello there!",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(call));
  loop.Run();

  ASSERT_EQ(results_.split_make_and_model_code,
            PpdProvider::CallbackResultCode::SUCCESS);
  EXPECT_THAT(results_.split_make, StrEq("General"));
  EXPECT_THAT(results_.split_model, StrEq("Kenobi"));
}

// Verifies that the manager fails the ReverseLookupCallback when it
// fails to fetch the necessary metadata from the Chrome OS Printing
// serving root.
TEST_F(PpdMetadataManagerTest, FailsToSplitMakeAndModelOnFetchFailure) {
  // Known interaction: asking the manager to split the string
  // "Hello there!" will cause it to fetch the reverse index metadata
  // with shard number 2.
  //
  // We elect _not_ to fake a value for this s.t. the fetch will fail.

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchSplitMakeAndModel,
                     base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::SplitMakeAndModel,
                             base::Unretained(manager_.get()), "Hello there!",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(call));
  loop.Run();

  EXPECT_EQ(results_.split_make_and_model_code,
            PpdProvider::CallbackResultCode::SERVER_ERROR);
}

// Verifies that the manager fails the ReverseLookupCallback when it
// fails to parse the necessary metadata from the Chrome OS Printing
// serving root.
TEST_F(PpdMetadataManagerTest, FailsToSplitMakeAndModelOnParseFailure) {
  // Known interaction: asking the manager to split the string
  // "Hello there!" will cause it to fetch the reverse index metadata
  // with shard number 2.
  //
  // We fake a fetch value that is invalid JSON s.t. the manager
  // will fail to parse it.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/reverse_index-en-02.json", kInvalidJson);

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchSplitMakeAndModel,
                     base::Unretained(this), loop.QuitClosure());
  auto call = base::BindOnce(&PpdMetadataManager::SplitMakeAndModel,
                             base::Unretained(manager_.get()), "Hello there!",
                             kArbitraryTimeDelta, std::move(callback));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(call));
  loop.Run();

  ASSERT_EQ(results_.split_make_and_model_code,
            PpdProvider::CallbackResultCode::INTERNAL_ERROR);
}

// Verifies that the manager fetches reverse index metadata anew when
// caller asks it for metadata fresher than what it has cached.
TEST_F(PpdMetadataManagerTest, CanSplitMakeAndModelTimeSensitive) {
  // Known interaction: asking the manager to split the string
  // "Hello there!" will cause it to fetch the reverse index metadata
  // with shard number 2.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/reverse_index-en-02.json", R"(
      {
        "reverseIndex": {
          "Hello there!": {
            "manufacturer": "General",
            "model": "Kenobi"
          }
        }
      }
  )");

  base::RunLoop loop;
  auto callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchSplitMakeAndModel,
                     base::Unretained(this), loop.QuitClosure());

  // t = 0s: caller requests that |manager_| split the
  // effective-make-and-model string "Hello there!" using metadata
  // parsed within the last thirty seconds. |manager_| fetches the
  // appropriate reverse index metadata.
  manager_->SplitMakeAndModel("Hello there!", base::Seconds(30),
                              std::move(callback));
  loop.Run();

  ASSERT_EQ(results_.split_make_and_model_code,
            PpdProvider::CallbackResultCode::SUCCESS);
  EXPECT_THAT(results_.split_make, StrEq("General"));
  EXPECT_THAT(results_.split_model, StrEq("Kenobi"));

  // Mutates the reverse index metadata we serve.
  GetFakeCache()->SetFetchResponseForTesting(
      "metadata_v3/reverse_index-en-02.json", R"(
      {
        "reverseIndex": {
          "Hello there!": {
            "manufacturer": "You are",
            "model": "a bold one!"
          }
        }
      }
  )");

  // Jams the result to a bad value, requiring that the |manager_|
  // answer us positively.
  results_.split_make_and_model_code =
      PpdProvider::CallbackResultCode::INTERNAL_ERROR;

  base::RunLoop second_loop;
  auto second_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchSplitMakeAndModel,
                     base::Unretained(this), second_loop.QuitClosure());

  // t = 0s: caller requests that |manager_| split the
  // effective-make-and-model string "Hello there!"
  // |manager_| re-uses the cached reverse index metadata.
  manager_->SplitMakeAndModel("Hello there!", base::Seconds(30),
                              std::move(second_callback));
  second_loop.Run();

  // We assert that the results are currently unchanged.
  ASSERT_EQ(results_.split_make_and_model_code,
            PpdProvider::CallbackResultCode::SUCCESS);
  EXPECT_THAT(results_.split_make, StrEq("General"));
  EXPECT_THAT(results_.split_model, StrEq("Kenobi"));

  // t = 31s
  clock_.Advance(base::Seconds(31));

  // Jams the result to a bad value, requiring that the |manager_|
  // answer us positively.
  results_.split_make_and_model_code =
      PpdProvider::CallbackResultCode::INTERNAL_ERROR;

  base::RunLoop third_loop;
  auto third_callback =
      base::BindOnce(&PpdMetadataManagerTest::CatchSplitMakeAndModel,
                     base::Unretained(this), third_loop.QuitClosure());

  // t = 31s: caller requests that |manager_| split the
  // effective-make-and-model string "Hello there!"
  // |manager_| doesn't have sufficiently fresh metadata, so it performs
  // a live fetch.
  manager_->SplitMakeAndModel("Hello there!", base::Seconds(30),
                              std::move(third_callback));
  third_loop.Run();

  // We assert that the live fetch changed the results.
  ASSERT_EQ(results_.split_make_and_model_code,
            PpdProvider::CallbackResultCode::SUCCESS);
  EXPECT_THAT(results_.split_make, StrEq("You are"));
  EXPECT_THAT(results_.split_model, StrEq("a bold one!"));
}

class PpdMetadataManagerBase : public ::testing::Test {
 public:
  explicit PpdMetadataManagerBase(PpdIndexChannel channel)
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    auto cache = std::make_unique<FakePrinterConfigCache>();
    cache_ = cache.get();
    manager_ = PpdMetadataManager::Create(channel, &clock_, std::move(cache));
  }
  ~PpdMetadataManagerBase() override = default;

 protected:

  bool CallGetManufacturers() {
    base::RunLoop loop;
    bool result;
    auto callback = [&loop, &result](PpdProvider::CallbackResultCode param1,
                                     const std::vector<std::string>& param2) {
      result = (param1 == PpdProvider::CallbackResultCode::SUCCESS);
      loop.Quit();
    };
    manager_->GetManufacturers(kArbitraryTimeDelta,
                               base::BindLambdaForTesting(callback));
    loop.Run();
    return result;
  }

  // Environment for task schedulers.
  base::test::TaskEnvironment task_environment_;
  // Controlled clock that dispenses times of Fetch().
  base::SimpleTestClock clock_;
  // Class under test.
  std::unique_ptr<PpdMetadataManager> manager_;
  raw_ptr<FakePrinterConfigCache> cache_;
};

class PpdMetadataManagerForStagingChannelTest : public PpdMetadataManagerBase {
 public:
  PpdMetadataManagerForStagingChannelTest()
      : PpdMetadataManagerBase(PpdIndexChannel::kStaging) {}
  ~PpdMetadataManagerForStagingChannelTest() override = default;
};

TEST_F(PpdMetadataManagerForStagingChannelTest, CanGetManufacturers) {
  cache_->SetFetchResponseForTesting(
      "metadata_v3_staging/manufacturers-en.json",
      R"({ "filesMap": {"It":"test-en.json"}})");

  EXPECT_TRUE(CallGetManufacturers());
}

class PpdMetadataManagerForDevChannelTest : public PpdMetadataManagerBase {
 public:
  PpdMetadataManagerForDevChannelTest()
      : PpdMetadataManagerBase(PpdIndexChannel::kDev) {}
  ~PpdMetadataManagerForDevChannelTest() override = default;
};

TEST_F(PpdMetadataManagerForDevChannelTest, CanGetManufacturers) {
  cache_->SetFetchResponseForTesting("metadata_v3_dev/manufacturers-en.json",
                                     R"({ "filesMap": {"It":"test-en.json"}})");

  EXPECT_TRUE(CallGetManufacturers());
}

}  // namespace
}  // namespace chromeos
