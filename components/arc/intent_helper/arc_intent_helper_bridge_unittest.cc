// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/intent_helper/open_url_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kPackageName[] = "default.package.name";

IntentFilter GetIntentFilter(const std::string& host) {
  std::vector<IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back(host, -1);
  return IntentFilter(kPackageName, std::move(authorities),
                      std::vector<IntentFilter::PatternMatcher>());
}

}  // namespace

class ArcIntentHelperTest : public testing::Test {
 protected:
  ArcIntentHelperTest() = default;

  class TestOpenUrlDelegate : public OpenUrlDelegate {
   public:
    ~TestOpenUrlDelegate() override = default;

    // OpenUrlDelegate:
    void OpenUrlFromArc(const GURL& url) override { last_opened_url_ = url; }
    void OpenWebAppFromArc(const GURL& url) override { last_opened_url_ = url; }

    GURL TakeLastOpenedUrl() {
      GURL result = std::move(last_opened_url_);
      last_opened_url_ = GURL();
      return result;
    }

   private:
    GURL last_opened_url_;
  };

  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<TestOpenUrlDelegate> test_open_url_delegate_;
  std::unique_ptr<ArcIntentHelperBridge> instance_;

 private:
  void SetUp() override {
    arc_bridge_service_ = std::make_unique<ArcBridgeService>();
    test_open_url_delegate_ = std::make_unique<TestOpenUrlDelegate>();
    instance_ = std::make_unique<ArcIntentHelperBridge>(
        nullptr /* context */, arc_bridge_service_.get());
    ArcIntentHelperBridge::SetOpenUrlDelegate(test_open_url_delegate_.get());
  }

  void TearDown() override {
    ArcIntentHelperBridge::SetOpenUrlDelegate(nullptr);
    instance_.reset();
    test_open_url_delegate_.reset();
    arc_bridge_service_.reset();
  }

  DISALLOW_COPY_AND_ASSIGN(ArcIntentHelperTest);
};

// Tests if IsIntentHelperPackage works as expected. Probably too trivial
// to test but just in case.
TEST_F(ArcIntentHelperTest, TestIsIntentHelperPackage) {
  EXPECT_FALSE(ArcIntentHelperBridge::IsIntentHelperPackage(""));
  EXPECT_FALSE(ArcIntentHelperBridge::IsIntentHelperPackage(
      ArcIntentHelperBridge::kArcIntentHelperPackageName + std::string("a")));
  EXPECT_FALSE(ArcIntentHelperBridge::IsIntentHelperPackage(
      ArcIntentHelperBridge::kArcIntentHelperPackageName +
      std::string("/.ArcIntentHelperActivity")));
  EXPECT_TRUE(ArcIntentHelperBridge::IsIntentHelperPackage(
      ArcIntentHelperBridge::kArcIntentHelperPackageName));
}

// Tests if FilterOutIntentHelper removes handlers as expected.
TEST_F(ArcIntentHelperTest, TestFilterOutIntentHelper) {
  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(0U, filtered.size());
  }

  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = "package_name0";
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = "package_name1";

    // FilterOutIntentHelper is no-op in this case.
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(2U, filtered.size());
  }

  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = "package_name1";

    // FilterOutIntentHelper should remove the first element.
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    ASSERT_EQ(1U, filtered.size());
    EXPECT_EQ("1", filtered[0]->name);
    EXPECT_EQ("package_name1", filtered[0]->package_name);
  }

  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = "package_name1";
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[2]->name = "2";
    orig[2]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;

    // FilterOutIntentHelper should remove two elements.
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    ASSERT_EQ(1U, filtered.size());
    EXPECT_EQ("1", filtered[0]->name);
    EXPECT_EQ("package_name1", filtered[0]->package_name);
  }

  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;

    // FilterOutIntentHelper should remove all elements.
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(0U, filtered.size());
  }
}

// Tests if observer works as expected.
TEST_F(ArcIntentHelperTest, TestObserver) {
  class FakeObserver : public ArcIntentHelperObserver {
   public:
    FakeObserver() = default;
    void OnIntentFiltersUpdated() override { updated_ = true; }
    bool IsUpdated() { return updated_; }
    void Reset() { updated_ = false; }

   private:
    bool updated_ = false;
  };

  // Observer should be called when intent filter is updated.
  auto observer = std::make_unique<FakeObserver>();
  instance_->AddObserver(observer.get());
  EXPECT_FALSE(observer->IsUpdated());
  instance_->OnIntentFiltersUpdated(std::vector<IntentFilter>());
  EXPECT_TRUE(observer->IsUpdated());

  // Observer should not be called after it's removed.
  observer->Reset();
  instance_->RemoveObserver(observer.get());
  instance_->OnIntentFiltersUpdated(std::vector<IntentFilter>());
  EXPECT_FALSE(observer->IsUpdated());
}

// Tests that ShouldChromeHandleUrl returns true by default.
TEST_F(ArcIntentHelperTest, TestDefault) {
  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("http://www.google.com")));
  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("https://www.google.com")));
  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("file:///etc/password")));
  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("chrome://help")));
  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("about://chrome")));
}

// Tests that ShouldChromeHandleUrl returns false when there's a match.
TEST_F(ArcIntentHelperTest, TestSingleFilter) {
  std::vector<IntentFilter> array;
  array.emplace_back(GetIntentFilter("www.google.com"));
  instance_->OnIntentFiltersUpdated(std::move(array));

  EXPECT_FALSE(instance_->ShouldChromeHandleUrl(GURL("http://www.google.com")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("https://www.google.com")));

  EXPECT_TRUE(
      instance_->ShouldChromeHandleUrl(GURL("https://www.google.co.uk")));
}

// Tests the same with multiple filters.
TEST_F(ArcIntentHelperTest, TestMultipleFilters) {
  std::vector<IntentFilter> array;
  array.emplace_back(GetIntentFilter("www.google.com"));
  array.emplace_back(GetIntentFilter("www.google.co.uk"));
  array.emplace_back(GetIntentFilter("dev.chromium.org"));
  instance_->OnIntentFiltersUpdated(std::move(array));

  EXPECT_FALSE(instance_->ShouldChromeHandleUrl(GURL("http://www.google.com")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("https://www.google.com")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("http://www.google.co.uk")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("https://www.google.co.uk")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("http://dev.chromium.org")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("https://dev.chromium.org")));

  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("http://www.android.com")));
}

// Tests that ShouldChromeHandleUrl returns true for non http(s) URLs.
TEST_F(ArcIntentHelperTest, TestNonHttp) {
  std::vector<IntentFilter> array;
  array.emplace_back(GetIntentFilter("www.google.com"));
  instance_->OnIntentFiltersUpdated(std::move(array));

  EXPECT_TRUE(
      instance_->ShouldChromeHandleUrl(GURL("chrome://www.google.com")));
  EXPECT_TRUE(
      instance_->ShouldChromeHandleUrl(GURL("custom://www.google.com")));
}

// Tests that ShouldChromeHandleUrl discards the previous filters when
// UpdateIntentFilters is called with new ones.
TEST_F(ArcIntentHelperTest, TestMultipleUpdate) {
  std::vector<IntentFilter> array;
  array.emplace_back(GetIntentFilter("www.google.com"));
  array.emplace_back(GetIntentFilter("dev.chromium.org"));
  instance_->OnIntentFiltersUpdated(std::move(array));

  std::vector<IntentFilter> array2;
  array2.emplace_back(GetIntentFilter("www.google.co.uk"));
  array2.emplace_back(GetIntentFilter("dev.chromium.org"));
  array2.emplace_back(GetIntentFilter("www.android.com"));
  instance_->OnIntentFiltersUpdated(std::move(array2));

  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("http://www.google.com")));
  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("https://www.google.com")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("http://www.google.co.uk")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("https://www.google.co.uk")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("http://dev.chromium.org")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("https://dev.chromium.org")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("http://www.android.com")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("https://www.android.com")));
}

// Tests that OnOpenUrl opens the URL in Chrome browser.
TEST_F(ArcIntentHelperTest, TestOnOpenUrl) {
  instance_->OnOpenUrl("http://google.com");
  EXPECT_EQ(GURL("http://google.com"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenUrl("https://google.com");
  EXPECT_EQ(GURL("https://google.com"),
            test_open_url_delegate_->TakeLastOpenedUrl());
}

// Tests that OnOpenWebApp opens only HTTPS URLs.
TEST_F(ArcIntentHelperTest, TestOnOpenWebApp) {
  instance_->OnOpenWebApp("http://google.com");
  EXPECT_EQ(GURL(), test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenWebApp("https://google.com");
  EXPECT_EQ(GURL("https://google.com"),
            test_open_url_delegate_->TakeLastOpenedUrl());
}

// Tests that OnOpenUrl does not open URLs with the 'chrome://' and equivalent
// schemes like 'about:'.
TEST_F(ArcIntentHelperTest, TestOnOpenUrl_ChromeScheme) {
  instance_->OnOpenUrl("chrome://www.google.com");
  EXPECT_FALSE(test_open_url_delegate_->TakeLastOpenedUrl().is_valid());

  instance_->OnOpenUrl("chrome://settings");
  EXPECT_FALSE(test_open_url_delegate_->TakeLastOpenedUrl().is_valid());

  instance_->OnOpenUrl("about:");
  EXPECT_FALSE(test_open_url_delegate_->TakeLastOpenedUrl().is_valid());

  instance_->OnOpenUrl("about:settings");
  EXPECT_FALSE(test_open_url_delegate_->TakeLastOpenedUrl().is_valid());

  instance_->OnOpenUrl("about:blank");
  EXPECT_FALSE(test_open_url_delegate_->TakeLastOpenedUrl().is_valid());
}

// Tests that OnOpenChromePage opens the specified settings section in the
// Chrome browser.
TEST_F(ArcIntentHelperTest, TestOnOpenChromePage) {
  instance_->OnOpenChromePage(mojom::ChromePage::MAIN);
  EXPECT_EQ(GURL("chrome://settings"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::MULTIDEVICE);
  EXPECT_EQ(GURL("chrome://settings/multidevice"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::WIFI);
  EXPECT_EQ(GURL("chrome://settings/networks/?type=WiFi"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::POWER);
  EXPECT_EQ(GURL("chrome://settings/power"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::BLUETOOTH);
  EXPECT_EQ(GURL("chrome://settings/bluetoothDevices"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::DATETIME);
  EXPECT_EQ(GURL("chrome://settings/dateTime"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::DISPLAY);
  EXPECT_EQ(GURL("chrome://settings/display"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::PRIVACY);
  EXPECT_EQ(GURL("chrome://settings/privacy"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::HELP);
  EXPECT_EQ(GURL("chrome://settings/help"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::ACCOUNTS);
  EXPECT_EQ(GURL("chrome://settings/accounts"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::APPEARANCE);
  EXPECT_EQ(GURL("chrome://settings/appearance"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::AUTOFILL);
  EXPECT_EQ(GURL("chrome://settings/autofill"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::BLUETOOTHDEVICES);
  EXPECT_EQ(GURL("chrome://settings/bluetoothDevices"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::CHANGEPICTURE);
  EXPECT_EQ(GURL("chrome://settings/changePicture"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::CLEARBROWSERDATA);
  EXPECT_EQ(GURL("chrome://settings/clearBrowserData"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::CLOUDPRINTERS);
  EXPECT_EQ(GURL("chrome://settings/cloudPrinters"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::CUPSPRINTERS);
  EXPECT_EQ(GURL("chrome://settings/cupsPrinters"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::DOWNLOADS);
  EXPECT_EQ(GURL("chrome://settings/downloads"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::ABOUTDOWNLOADS);
  EXPECT_EQ(GURL("about:downloads"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::ABOUTHISTORY);
  EXPECT_EQ(GURL("about:history"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::KEYBOARDOVERLAY);
  EXPECT_EQ(GURL("chrome://settings/keyboard-overlay"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::LANGUAGES);
  EXPECT_EQ(GURL("chrome://settings/languages"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::LOCKSCREEN);
  EXPECT_EQ(GURL("chrome://settings/lockScreen"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::MANAGEACCESSIBILITY);
  EXPECT_EQ(GURL("chrome://settings/manageAccessibility"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::NETWORKSTYPEVPN);
  EXPECT_EQ(GURL("chrome://settings/networks?type=VPN"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::ONSTARTUP);
  EXPECT_EQ(GURL("chrome://settings/onStartup"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::PASSWORDS);
  EXPECT_EQ(GURL("chrome://settings/passwords"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::POINTEROVERLAY);
  EXPECT_EQ(GURL("chrome://settings/pointer-overlay"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::RESET);
  EXPECT_EQ(GURL("chrome://settings/reset"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::SEARCH);
  EXPECT_EQ(GURL("chrome://settings/search"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::STORAGE);
  EXPECT_EQ(GURL("chrome://settings/storage"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::SYNCSETUP);
  EXPECT_EQ(GURL("chrome://settings/syncSetup"),
            test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenChromePage(mojom::ChromePage::ABOUTBLANK);
  EXPECT_EQ(GURL("about:blank"), test_open_url_delegate_->TakeLastOpenedUrl());
}

// Tests that AppendStringToIntentHelperPackageName works.
TEST_F(ArcIntentHelperTest, TestAppendStringToIntentHelperPackageName) {
  std::string package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
  std::string fake_activity = "this_is_a_fake_activity";
  EXPECT_EQ(ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
                fake_activity),
            package_name + "." + fake_activity);

  std::string empty_string = "";
  EXPECT_EQ(ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
                empty_string),
            package_name + ".");
}

}  // namespace arc
