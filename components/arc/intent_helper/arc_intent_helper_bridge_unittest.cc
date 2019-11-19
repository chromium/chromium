// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "components/arc/intent_helper/open_url_delegate.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kPackageName[] = "default.package.name";

IntentFilter GetIntentFilter(const std::string& host,
                             const std::string& pkg_name) {
  std::vector<IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back(host, /*port=*/-1);
  return IntentFilter(pkg_name, std::move(authorities),
                      std::vector<IntentFilter::PatternMatcher>(),
                      std::vector<std::string>());
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
    void OpenArcCustomTab(
        const GURL& url,
        int32_t task_id,
        int32_t surface_id,
        int32_t top_margin,
        mojom::IntentHelperHost::OnOpenCustomTabCallback callback) override {
      std::move(callback).Run(nullptr);
    }
    void OpenChromePageFromArc(mojom::ChromePage chrome_page) override {}

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
    void OnIntentFiltersUpdated(
        const base::Optional<std::string>& package_name) override {
      updated_ = true;
    }
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
  array.emplace_back(GetIntentFilter("www.google.com", kPackageName));
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
  array.emplace_back(GetIntentFilter("www.google.com", kPackageName));
  array.emplace_back(GetIntentFilter("www.google.co.uk", kPackageName));
  array.emplace_back(GetIntentFilter("dev.chromium.org", kPackageName));
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
  array.emplace_back(GetIntentFilter("www.google.com", kPackageName));
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
  array.emplace_back(GetIntentFilter("www.google.com", kPackageName));
  array.emplace_back(GetIntentFilter("dev.chromium.org", kPackageName));
  instance_->OnIntentFiltersUpdated(std::move(array));

  std::vector<IntentFilter> array2;
  array2.emplace_back(GetIntentFilter("www.google.co.uk", kPackageName));
  array2.emplace_back(GetIntentFilter("dev.chromium.org", kPackageName));
  array2.emplace_back(GetIntentFilter("www.android.com", kPackageName));
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

// Tests that intent helper app (on ARC) is not taken as an app candidate, other
// suitable app candidates should still match if possible.
TEST_F(ArcIntentHelperTest, TestIntentHelperAppIsNotAValidCandidate) {
  std::vector<IntentFilter> array;
  array.emplace_back(GetIntentFilter(
      "www.google.com", ArcIntentHelperBridge::kArcIntentHelperPackageName));
  array.emplace_back(GetIntentFilter(
      "www.android.com", ArcIntentHelperBridge::kArcIntentHelperPackageName));
  // Let the package name start with "z" to ensure the intent helper package
  // is not always the last package checked in the ShouldChromeHandleUrl
  // filter matching logic. This is to ensure this unit test tests the package
  // name checking logic properly.
  array.emplace_back(GetIntentFilter("dev.chromium.org", "z.package.name"));
  instance_->OnIntentFiltersUpdated(std::move(array));

  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("http://www.google.com")));
  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("https://www.google.com")));
  EXPECT_TRUE(instance_->ShouldChromeHandleUrl(GURL("http://www.android.com")));
  EXPECT_TRUE(
      instance_->ShouldChromeHandleUrl(GURL("https://www.android.com")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("http://dev.chromium.org")));
  EXPECT_FALSE(
      instance_->ShouldChromeHandleUrl(GURL("https://dev.chromium.org")));
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

// Tests that AppendStringToIntentHelperPackageName works.
TEST_F(ArcIntentHelperTest, TestAppendStringToIntentHelperPackageName) {
  std::string package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
  std::string fake_activity = "this_is_a_fake_activity";
  EXPECT_EQ(ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
                fake_activity),
            package_name + "." + fake_activity);

  const std::string empty_string;
  EXPECT_EQ(ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
                empty_string),
            package_name + ".");
}

}  // namespace arc
