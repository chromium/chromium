// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rlz/rlz_tracker.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/rlz/rlz_tracker_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "rlz/test/rlz_test_helpers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_IOS)
#include "ui/base/device_form_factor.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::AssertionFailure;

namespace rlz {
namespace {

class TestRLZTrackerDelegate : public RLZTrackerDelegate {
 public:
  TestRLZTrackerDelegate()
      : request_context_getter_(new net::TestURLRequestContextGetter(
            base::SingleThreadTaskRunner::GetCurrentDefault())) {}

  TestRLZTrackerDelegate(const TestRLZTrackerDelegate&) = delete;
  TestRLZTrackerDelegate& operator=(const TestRLZTrackerDelegate&) = delete;

  void set_brand(const char* brand) { brand_override_ = brand; }

  void set_reactivation_brand(const char* reactivation_brand) {
    // TODO(thakis): Reactivation doesn't exist on Mac yet.
    reactivation_brand_override_ = reactivation_brand;
  }

  void SimulateOmniboxUsage() {
    if (!on_omnibox_search_callback_.is_null())
      std::move(on_omnibox_search_callback_).Run();
  }

  void SimulateHomepageUsage() {
    if (!on_homepage_search_callback_.is_null())
      std::move(on_homepage_search_callback_).Run();
  }

  // RLZTrackerDelegate implementation.
  void Cleanup() override {
    on_omnibox_search_callback_.Reset();
    on_homepage_search_callback_.Reset();
  }

  bool IsOnUIThread() override { return true; }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    NOTIMPLEMENTED() << "If this is called, it needs an implementation.";
    return nullptr;
  }

  bool GetBrand(std::string* brand) override {
    *brand = brand_override_;
    return true;
  }

  bool IsBrandOrganic(const std::string& brand) override {
    return brand.empty() || brand == "GGLS" || brand == "GGRS";
  }

  bool GetReactivationBrand(std::string* brand) override {
    *brand = reactivation_brand_override_;
    return true;
  }

  bool ShouldEnableZeroDelayForTesting() override { return true; }

  bool GetLanguage(std::u16string* language) override { return true; }

  bool GetReferral(std::u16string* referral) override { return true; }

  bool ClearReferral() override { return true; }

  void SetOmniboxSearchCallback(base::OnceClosure callback) override {
    DCHECK(!callback.is_null());
    on_omnibox_search_callback_ = std::move(callback);
  }

  void SetHomepageSearchCallback(base::OnceClosure callback) override {
    DCHECK(!callback.is_null());
    on_homepage_search_callback_ = std::move(callback);
  }

  void RunHomepageSearchCallback() override {
    if (!on_homepage_search_callback_.is_null()) {
      std::move(on_homepage_search_callback_).Run();
    }
  }

  // A speculative fix for https://crbug.com/907379.
  bool ShouldUpdateExistingAccessPointRlz() override { return false; }

 private:
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  std::string brand_override_;
  std::string reactivation_brand_override_;
  base::OnceClosure on_omnibox_search_callback_;
  base::OnceClosure on_homepage_search_callback_;
};

// Dummy RLZ string for the access points.
const char kOmniboxRlzString[] = "test_omnibox";
const char kNewOmniboxRlzString[] = "new_omnibox";
#if !BUILDFLAG(IS_IOS)
const char kHomepageRlzString[] = "test_homepage";
const char kNewHomepageRlzString[] = "new_homepage";
const char kAppListRlzString[] = "test_applist";
const char kNewAppListRlzString[] = "new_applist";
#endif  // !BUILDFLAG(IS_IOS)

// Some helper macros to test it a string contains/does not contain a substring.

AssertionResult CmpHelperSTRC(const char* str_expression,
                              const char* substr_expression,
                              const char* str,
                              const char* substr) {
  if (nullptr != strstr(str, substr)) {
    return AssertionSuccess();
  }

  return AssertionFailure() << "Expected: (" << substr_expression << ") in ("
                            << str_expression << "), actual: '"
                            << substr << "' not in '" << str << "'";
}

AssertionResult CmpHelperSTRNC(const char* str_expression,
                               const char* substr_expression,
                               const char* str,
                               const char* substr) {
  if (nullptr == strstr(str, substr)) {
    return AssertionSuccess();
  }

  return AssertionFailure() << "Expected: (" << substr_expression
                            << ") not in (" << str_expression << "), actual: '"
                            << substr << "' in '" << str << "'";
}

#define EXPECT_STR_CONTAINS(str, substr) \
    EXPECT_PRED_FORMAT2(CmpHelperSTRC, str, substr)

#define EXPECT_STR_NOT_CONTAIN(str, substr) \
    EXPECT_PRED_FORMAT2(CmpHelperSTRNC, str, substr)

}  // namespace

// Test class for RLZ tracker. Makes some member functions public and
// overrides others to make it easier to test.
class TestRLZTracker : public RLZTracker {
 public:
  using RLZTracker::InitRlzDelayed;
  using RLZTracker::DelayedInit;

  TestRLZTracker() : assume_not_ui_thread_(true) { set_tracker(this); }

  TestRLZTracker(const TestRLZTracker&) = delete;
  TestRLZTracker& operator=(const TestRLZTracker&) = delete;

  ~TestRLZTracker() override { set_tracker(nullptr); }

  bool was_ping_sent_for_brand(const std::string& brand) const {
    return pinged_brands_.count(brand) > 0;
  }

  void set_assume_not_ui_thread(bool assume_not_ui_thread) {
    assume_not_ui_thread_ = assume_not_ui_thread;
  }

 private:
  void ScheduleDelayedInit(base::TimeDelta delay) override {
    // If the delay is 0, invoke the delayed init now. Otherwise,
    // don't schedule anything, it will be manually called during tests.
    if (delay.is_zero())
      DelayedInit();
  }

  void ScheduleFinancialPing() override { PingNowImpl(); }

  bool ScheduleRecordProductEvent(rlz_lib::Product product,
                                  rlz_lib::AccessPoint point,
                                  rlz_lib::Event event_id) override {
    return !assume_not_ui_thread_;
  }

  bool ScheduleGetAccessPointRlz(rlz_lib::AccessPoint point) override {
    return !assume_not_ui_thread_;
  }

  bool ScheduleRecordFirstSearch(rlz_lib::AccessPoint point) override {
    return !assume_not_ui_thread_;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool ScheduleClearRlzState() override { return !assume_not_ui_thread_; }
#endif

  bool SendFinancialPing(const std::string& brand,
                         const std::u16string& lang,
                         const std::u16string& referral) override {
    // Don't ping the server during tests, just pretend as if we did.
    EXPECT_FALSE(brand.empty());
    pinged_brands_.insert(brand);

    // Set new access points RLZ string, like the actual server ping would have
    // done.
    rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(),
                               kNewOmniboxRlzString);
#if !BUILDFLAG(IS_IOS)
    rlz_lib::SetAccessPointRlz(RLZTracker::ChromeHomePage(),
                               kNewHomepageRlzString);
    rlz_lib::SetAccessPointRlz(RLZTracker::ChromeAppList(),
                               kNewAppListRlzString);
#endif  // !BUILDFLAG(IS_IOS)
    return true;
  }

  std::set<std::string> pinged_brands_;
  bool assume_not_ui_thread_;
};

class RlzLibTest : public testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  void SetMainBrand(const char* brand);
  void SetReactivationBrand(const char* brand);

  void SimulateOmniboxUsage();
  void SimulateHomepageUsage();
  void SimulateAppListUsage();
  void InvokeDelayedInit();

  void ExpectEventRecorded(const char* event_name, bool expected);
  void ExpectRlzPingSent(bool expected);
  void ExpectReactivationRlzPingSent(bool expected);

  base::test::TaskEnvironment task_environment_;
  raw_ptr<TestRLZTrackerDelegate> delegate_;
  std::unique_ptr<TestRLZTracker> tracker_;
  RlzLibTestNoMachineStateHelper m_rlz_test_helper_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::system::FakeStatisticsProvider> statistics_provider_;
#endif
};

void RlzLibTest::SetUp() {
  testing::Test::SetUp();
  m_rlz_test_helper_.SetUp();

  delegate_ = new TestRLZTrackerDelegate;
  tracker_ = std::make_unique<TestRLZTracker>();
  RLZTracker::SetRlzDelegate(base::WrapUnique(delegate_.get()));

  // Make sure a non-organic brand code is set in the registry or the RLZTracker
  // is pretty much a no-op.
  SetMainBrand("TEST");
  SetReactivationBrand("");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  statistics_provider_ =
      std::make_unique<ash::system::FakeStatisticsProvider>();
  ash::system::StatisticsProvider::SetTestProvider(statistics_provider_.get());
  statistics_provider_->SetMachineStatistic(
      ash::system::kShouldSendRlzPingKey,
      ash::system::kShouldSendRlzPingValueTrue);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void RlzLibTest::TearDown() {
  delegate_ = nullptr;
  tracker_.reset();
  testing::Test::TearDown();
  m_rlz_test_helper_.TearDown();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::StatisticsProvider::SetTestProvider(nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void RlzLibTest::SetMainBrand(const char* brand) {
  delegate_->set_brand(brand);
}

void RlzLibTest::SetReactivationBrand(const char* brand) {
  delegate_->set_reactivation_brand(brand);
}

void RlzLibTest::SimulateOmniboxUsage() {
  delegate_->SimulateOmniboxUsage();
}

void RlzLibTest::SimulateHomepageUsage() {
  delegate_->SimulateHomepageUsage();
}

void RlzLibTest::SimulateAppListUsage() {
#if !BUILDFLAG(IS_IOS)
  RLZTracker::RecordAppListSearch();
#endif  // !BUILDFLAG(IS_IOS)
}

void RlzLibTest::InvokeDelayedInit() {
  tracker_->DelayedInit();
}

void RlzLibTest::ExpectEventRecorded(const char* event_name, bool expected) {
  char cgi[rlz_lib::kMaxCgiLength];
  GetProductEventsAsCgi(rlz_lib::CHROME, cgi, std::size(cgi));
  if (expected) {
    EXPECT_STR_CONTAINS(cgi, event_name);
  } else {
    EXPECT_STR_NOT_CONTAIN(cgi, event_name);
  }
}

void RlzLibTest::ExpectRlzPingSent(bool expected) {
  std::string brand;
  delegate_->GetBrand(&brand);
  EXPECT_EQ(expected, tracker_->was_ping_sent_for_brand(brand));
}

void RlzLibTest::ExpectReactivationRlzPingSent(bool expected) {
  std::string brand;
  delegate_->GetReactivationBrand(&brand);
  EXPECT_EQ(expected, tracker_->was_ping_sent_for_brand(brand));
}

// The events that affect the different RLZ scenarios are the following:
//
//  A: the user starts chrome for the first time
//  B: the user stops chrome
//  C: the user start a subsequent time
//  D: the user stops chrome again
//  I: the RLZTracker::DelayedInit() method is invoked
//  X: the user performs a search using the omnibox
//  Y: the user performs a search using the home page
//  Z: the user performs a search using the app list
//
// The events A to D happen in chronological order, but the other events
// may happen at any point between A-B or C-D, in no particular order.
//
// The visible results of the scenarios on Win are:
//
//  C1I event is recorded
//  C2I event is recorded
//  C7I event is recorded
//  C1F event is recorded
//  C2F event is recorded
//  C7F event is recorded
//  C1S event is recorded
//  C2S event is recorded
//  C7S event is recorded
//  RLZ ping sent
//
//  On Mac, C5 / C6 / C8 are sent instead of C1 / C2 / C7.
//  On ChromeOS, CA / CB / CC are sent, respectively.
//
//  On iOS, only the omnibox events are recorded, and the value send depends
//  on the device form factor (phone or tablet).
//
// Variations on the above scenarios:
//
//  - if the delay specified to InitRlzDelayed() is negative, then the RLZ
//    ping should be sent out at the time of event X and not wait for I
//
// Also want to test that pre-warming the RLZ string cache works correctly.

#if BUILDFLAG(IS_WIN)
const char kOmniboxInstall[] = "C1I";
const char kOmniboxSetToGoogle[] = "C1S";
const char kOmniboxFirstSearch[] = "C1F";

const char kHomepageInstall[] = "C2I";
const char kHomepageSetToGoogle[] = "C2S";
const char kHomepageFirstSearch[] = "C2F";

const char kAppListInstall[] = "C7I";
const char kAppListSetToGoogle[] = "C7S";
const char kAppListFirstSearch[] = "C7F";
#elif BUILDFLAG(IS_IOS)
const char kOmniboxInstallPhone[] = "CDI";
const char kOmniboxSetToGooglePhone[] = "CDS";
const char kOmniboxFirstSearchPhone[] = "CDF";

const char kOmniboxInstallTablet[] = "C9I";
const char kOmniboxSetToGoogleTablet[] = "C9S";
const char kOmniboxFirstSearchTablet[] = "C9F";
#elif BUILDFLAG(IS_MAC)
const char kOmniboxInstall[] = "C5I";
const char kOmniboxSetToGoogle[] = "C5S";
const char kOmniboxFirstSearch[] = "C5F";

const char kHomepageInstall[] = "C6I";
const char kHomepageSetToGoogle[] = "C6S";
const char kHomepageFirstSearch[] = "C6F";

const char kAppListInstall[] = "C8I";
const char kAppListSetToGoogle[] = "C8S";
const char kAppListFirstSearch[] = "C8F";
#elif BUILDFLAG(IS_CHROMEOS_ASH)
const char kOmniboxInstall[] = "CAI";
const char kOmniboxSetToGoogle[] = "CAS";
const char kOmniboxFirstSearch[] = "CAF";

const char kHomepageInstall[] = "CBI";
const char kHomepageSetToGoogle[] = "CBS";
const char kHomepageFirstSearch[] = "CBF";

const char kAppListInstall[] = "CCI";
const char kAppListSetToGoogle[] = "CCS";
const char kAppListFirstSearch[] = "CCF";
#endif

const char* OmniboxInstall() {
#if BUILDFLAG(IS_IOS)
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? kOmniboxInstallTablet
             : kOmniboxInstallPhone;
#else
  return kOmniboxInstall;
#endif
}

const char* OmniboxSetToGoogle() {
#if BUILDFLAG(IS_IOS)
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? kOmniboxSetToGoogleTablet
             : kOmniboxSetToGooglePhone;
#else
  return kOmniboxSetToGoogle;
#endif
}

const char* OmniboxFirstSearch() {
#if BUILDFLAG(IS_IOS)
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? kOmniboxFirstSearchTablet
             : kOmniboxFirstSearchPhone;
#else
  return kOmniboxFirstSearch;
#endif
}

const base::TimeDelta kDelay = base::Milliseconds(20);

TEST_F(RlzLibTest, RecordProductEvent) {
  RLZTracker::RecordProductEvent(rlz_lib::CHROME, RLZTracker::ChromeOmnibox(),
                                 rlz_lib::FIRST_SEARCH);

  ExpectEventRecorded(OmniboxFirstSearch(), true);
}

TEST_F(RlzLibTest, QuickStopAfterStart) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, true);

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), false);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSearch, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, DelayedInitOnly) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), true);
  ExpectEventRecorded(OmniboxSetToGoogle(), true);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSearch, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyGoogleAsStartup) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, false, false, true);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), true);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSearch, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoFirstRunNoRlzStrings) {
  TestRLZTracker::InitRlzDelayed(false, false, kDelay, true, true, false);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), true);
  ExpectEventRecorded(OmniboxSetToGoogle(), true);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSearch, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoFirstRunNoRlzStringsGoogleAsStartup) {
  TestRLZTracker::InitRlzDelayed(false, false, kDelay, false, false, true);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), true);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSearch, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoFirstRun) {
  // Set some dummy RLZ strings to simulate that we already ran before and
  // performed a successful ping to the RLZ server.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);
#if !BUILDFLAG(IS_IOS)
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeHomePage(), kHomepageRlzString);
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeAppList(), kAppListRlzString);
#endif  // !BUILDFLAG(IS_IOS)

  TestRLZTracker::InitRlzDelayed(false, false, kDelay, true, true, true);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), true);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSearch, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, DelayedInitOnlyNoGoogleDefaultSearchOrHomepageOrStartup) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, false, false, false);
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), true);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSearch, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, OmniboxUsageOnly) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  SimulateOmniboxUsage();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), false);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), true);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSearch, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, HomepageUsageOnly) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  SimulateHomepageUsage();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), false);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSearch, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, AppListUsageOnly) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  SimulateAppListUsage();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), false);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSearch, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, true);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, UsageBeforeDelayedInit) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  SimulateOmniboxUsage();
  SimulateHomepageUsage();
  SimulateAppListUsage();
  InvokeDelayedInit();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), true);
  ExpectEventRecorded(OmniboxSetToGoogle(), true);
  ExpectEventRecorded(OmniboxFirstSearch(), true);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSearch, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, true);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, UsageAfterDelayedInit) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();
  SimulateOmniboxUsage();
  SimulateHomepageUsage();
  SimulateAppListUsage();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), true);
  ExpectEventRecorded(OmniboxSetToGoogle(), true);
  ExpectEventRecorded(OmniboxFirstSearch(), true);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSearch, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, true);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, OmniboxUsageSendsPingWhenSendPingImmediately) {
  TestRLZTracker::InitRlzDelayed(true, true, kDelay, true, true, false);
  SimulateOmniboxUsage();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), true);
  ExpectEventRecorded(OmniboxSetToGoogle(), true);
  ExpectEventRecorded(OmniboxFirstSearch(), true);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, true);
  ExpectEventRecorded(kHomepageSetToGoogle, true);
  ExpectEventRecorded(kHomepageFirstSearch, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, true);
  ExpectEventRecorded(kAppListSetToGoogle, true);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(true);
}

TEST_F(RlzLibTest, HomepageUsageDoesNotSendPingWhenSendPingImmediately) {
  TestRLZTracker::InitRlzDelayed(true, true, kDelay, true, true, false);
  SimulateHomepageUsage();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), false);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSearch, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, StartupUsageDoesNotSendPingWhenSendPingImmediately) {
  TestRLZTracker::InitRlzDelayed(true, true, kDelay, true, false, true);
  SimulateHomepageUsage();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), false);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSearch, true);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, false);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, AppListUsageDoesNotSendPingWhenSendPingImmediately) {
  TestRLZTracker::InitRlzDelayed(true, true, kDelay, true, false, false);
  SimulateAppListUsage();

  // Omnibox events.
  ExpectEventRecorded(OmniboxInstall(), false);
  ExpectEventRecorded(OmniboxSetToGoogle(), false);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

#if !BUILDFLAG(IS_IOS)
  // Home page events.
  ExpectEventRecorded(kHomepageInstall, false);
  ExpectEventRecorded(kHomepageSetToGoogle, false);
  ExpectEventRecorded(kHomepageFirstSearch, false);

  // App list events.
  ExpectEventRecorded(kAppListInstall, false);
  ExpectEventRecorded(kAppListSetToGoogle, false);
  ExpectEventRecorded(kAppListFirstSearch, true);
#endif  // !BUILDFLAG(IS_IOS)

  ExpectRlzPingSent(false);
}

TEST_F(RlzLibTest, GetAccessPointRlzOnIoThread) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);

  std::u16string rlz;

  tracker_->set_assume_not_ui_thread(true);
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
}

TEST_F(RlzLibTest, GetAccessPointRlzNotOnIoThread) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);

  std::u16string rlz;

  tracker_->set_assume_not_ui_thread(false);
  EXPECT_FALSE(
      RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
}

TEST_F(RlzLibTest, GetAccessPointRlzIsCached) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);

  std::u16string rlz;

  tracker_->set_assume_not_ui_thread(false);
  EXPECT_FALSE(
      RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));

  tracker_->set_assume_not_ui_thread(true);
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());

  tracker_->set_assume_not_ui_thread(false);
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// By design, on Chrome OS the RLZ string can only be set once.  Once set,
// pings cannot change int.
TEST_F(RlzLibTest, PingUpdatesRlzCache) {
  // Set dummy RLZ string.
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeOmnibox(), kOmniboxRlzString);
#if !BUILDFLAG(IS_IOS)
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeHomePage(), kHomepageRlzString);
  rlz_lib::SetAccessPointRlz(RLZTracker::ChromeAppList(), kAppListRlzString);
#endif  // !BUILDFLAG(IS_IOS)

  std::u16string rlz;

  // Prime the cache.
  tracker_->set_assume_not_ui_thread(true);

  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
#if !BUILDFLAG(IS_IOS)
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(
        RLZTracker::ChromeHomePage(), &rlz));
  EXPECT_STREQ(kHomepageRlzString, base::UTF16ToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeAppList(), &rlz));
  EXPECT_STREQ(kAppListRlzString, base::UTF16ToUTF8(rlz).c_str());
#endif  // !BUILDFLAG(IS_IOS)

  // Make sure cache is valid.
  tracker_->set_assume_not_ui_thread(false);

  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
#if !BUILDFLAG(IS_IOS)
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(
        RLZTracker::ChromeHomePage(), &rlz));
  EXPECT_STREQ(kHomepageRlzString, base::UTF16ToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeAppList(), &rlz));
  EXPECT_STREQ(kAppListRlzString, base::UTF16ToUTF8(rlz).c_str());
#endif  // !BUILDFLAG(IS_IOS)

  // Perform ping.
  tracker_->set_assume_not_ui_thread(true);
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();
  ExpectRlzPingSent(true);

  // Make sure cache is now updated.
  tracker_->set_assume_not_ui_thread(false);

  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), &rlz));
  EXPECT_STREQ(kNewOmniboxRlzString, base::UTF16ToUTF8(rlz).c_str());
#if !BUILDFLAG(IS_IOS)
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(
        RLZTracker::ChromeHomePage(), &rlz));
  EXPECT_STREQ(kNewHomepageRlzString, base::UTF16ToUTF8(rlz).c_str());
  EXPECT_TRUE(RLZTracker::GetAccessPointRlz(RLZTracker::ChromeAppList(), &rlz));
  EXPECT_STREQ(kNewAppListRlzString, base::UTF16ToUTF8(rlz).c_str());
#endif  // !BUILDFLAG(IS_IOS)
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(thakis): Reactivation doesn't exist on Mac yet.
TEST_F(RlzLibTest, ReactivationNonOrganicNonOrganic) {
  SetReactivationBrand("REAC");

  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  ExpectRlzPingSent(true);
  ExpectReactivationRlzPingSent(true);
}

TEST_F(RlzLibTest, ReactivationOrganicNonOrganic) {
  SetMainBrand("GGLS");
  SetReactivationBrand("REAC");

  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  ExpectRlzPingSent(false);
  ExpectReactivationRlzPingSent(true);
}

TEST_F(RlzLibTest, ReactivationNonOrganicOrganic) {
  SetMainBrand("TEST");
  SetReactivationBrand("GGLS");

  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  ExpectRlzPingSent(true);
  ExpectReactivationRlzPingSent(false);
}

TEST_F(RlzLibTest, ReactivationOrganicOrganic) {
  SetMainBrand("GGLS");
  SetReactivationBrand("GGRS");

  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  InvokeDelayedInit();

  ExpectRlzPingSent(false);
  ExpectReactivationRlzPingSent(false);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(RlzLibTest, ClearRlzState) {
  RLZTracker::RecordProductEvent(rlz_lib::CHROME, RLZTracker::ChromeOmnibox(),
                                 rlz_lib::FIRST_SEARCH);

  ExpectEventRecorded(OmniboxFirstSearch(), true);

  RLZTracker::ClearRlzState();

  ExpectEventRecorded(OmniboxFirstSearch(), false);
}

TEST_F(RlzLibTest, DoNotRecordEventUnlessShouldSendRlzPingKeyIsTrue) {
  // Verify the event is recorded when |kShouldSendRlzPingKey| is true.
  ASSERT_EQ(statistics_provider_->GetMachineStatistic(
                ash::system::kShouldSendRlzPingKey),
            ash::system::kShouldSendRlzPingValueTrue);
  RLZTracker::RecordProductEvent(rlz_lib::CHROME, RLZTracker::ChromeOmnibox(),
                                 rlz_lib::FIRST_SEARCH);
  ExpectEventRecorded(OmniboxFirstSearch(), true);

  // Verify the event is not recorded when |kShouldSendRlzPingKey| is false.
  RLZTracker::ClearRlzState();
  ExpectEventRecorded(OmniboxFirstSearch(), false);
  statistics_provider_->SetMachineStatistic(
      ash::system::kShouldSendRlzPingKey,
      ash::system::kShouldSendRlzPingValueFalse);
  ASSERT_EQ(statistics_provider_->GetMachineStatistic(
                ash::system::kShouldSendRlzPingKey),
            ash::system::kShouldSendRlzPingValueFalse);
  RLZTracker::RecordProductEvent(rlz_lib::CHROME, RLZTracker::ChromeOmnibox(),
                                 rlz_lib::FIRST_SEARCH);
  ExpectEventRecorded(OmniboxFirstSearch(), false);

  // Verify the event is not recorded when |kShouldSendRlzPingKey| does not
  // exist.
  statistics_provider_->ClearMachineStatistic(
      ash::system::kShouldSendRlzPingKey);
  ASSERT_FALSE(statistics_provider_->GetMachineStatistic(
      ash::system::kShouldSendRlzPingKey));
  RLZTracker::RecordProductEvent(rlz_lib::CHROME, RLZTracker::ChromeOmnibox(),
                                 rlz_lib::FIRST_SEARCH);
  ExpectEventRecorded(OmniboxFirstSearch(), false);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_IOS)
TEST_F(RlzLibTest, RecordChromeHomePageSearch) {
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  EXPECT_TRUE(TestRLZTracker::ShouldRecordChromeHomePageSearch());

  TestRLZTracker::RecordChromeHomePageSearch();
  EXPECT_FALSE(TestRLZTracker::ShouldRecordChromeHomePageSearch());
  ExpectEventRecorded(kHomepageFirstSearch, true);
}

TEST_F(RlzLibTest, ShouldNotRecordChromeHomePageSearch) {
  SetMainBrand("GGLS");
  TestRLZTracker::InitRlzDelayed(true, false, kDelay, true, true, false);
  EXPECT_FALSE(TestRLZTracker::ShouldRecordChromeHomePageSearch());
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace rlz
