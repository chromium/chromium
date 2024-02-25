// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "ash/components/arc/mojom/intent_helper.mojom-shared.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/arc/intent_helper/open_url_delegate.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcIntentHelperTest : public testing::Test {
 protected:
  ArcIntentHelperTest() = default;
  ArcIntentHelperTest(const ArcIntentHelperTest&) = delete;
  ArcIntentHelperTest& operator=(const ArcIntentHelperTest&) = delete;

  class TestOpenUrlDelegate : public OpenUrlDelegate {
   public:
    ~TestOpenUrlDelegate() override = default;

    // OpenUrlDelegate:
    void OpenUrlFromArc(const GURL& url) override { last_opened_url_ = url; }
    void OpenWebAppFromArc(const GURL& url) override { last_opened_url_ = url; }
    void OpenArcCustomTab(
        const GURL& url,
        int32_t task_id,
        mojom::IntentHelperHost::OnOpenCustomTabCallback callback) override {
      std::move(callback).Run(mojo::NullRemote());
    }
    void OpenChromePageFromArc(mojom::ChromePage chrome_page) override {}
    void OpenAppWithIntent(const GURL& url,
                           mojom::LaunchIntentPtr intent) override {
      last_opened_url_ = url;
      last_opened_intent_ = std::move(intent);
    }

    GURL TakeLastOpenedUrl() {
      GURL result = std::move(last_opened_url_);
      last_opened_url_ = GURL();
      return result;
    }

    mojom::LaunchIntentPtr TakeLastOpenedIntent() {
      auto result = std::move(last_opened_intent_);
      last_opened_intent_.reset();
      return result;
    }

   private:
    GURL last_opened_url_;
    mojom::LaunchIntentPtr last_opened_intent_;
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
};

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
    orig[0]->package_name = kArcIntentHelperPackageName;
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
    orig[0]->package_name = kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = "package_name1";
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[2]->name = "2";
    orig[2]->package_name = kArcIntentHelperPackageName;

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
    orig[0]->package_name = kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = kArcIntentHelperPackageName;

    // FilterOutIntentHelper should remove all elements.
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(0U, filtered.size());
  }
}

// Tests if observer works as expected.
TEST_F(ArcIntentHelperTest, TestObserver) {
  class MockObserver : public ArcIntentHelperObserver {
   public:
    MOCK_METHOD(void,
                OnIntentFiltersUpdated,
                (const std::optional<std::string>& package_name),
                (override));
    MOCK_METHOD(void,
                OnArcSupportedLinksChanged,
                (const std::vector<arc::mojom::SupportedLinksPackagePtr>&
                     added_packages,
                 const std::vector<arc::mojom::SupportedLinksPackagePtr>&
                     removed_packages,
                 arc::mojom::SupportedLinkChangeSource source),
                (override));
  };

  // Create and add observer.
  testing::StrictMock<MockObserver> observer;
  instance_->AddObserver(&observer);

  {
    // Observer should be called when an intent filter is updated.
    EXPECT_CALL(observer, OnIntentFiltersUpdated(testing::Eq(std::nullopt)));
    instance_->OnIntentFiltersUpdated(/*filters=*/std::vector<IntentFilter>());
    testing::Mock::VerifyAndClearExpectations(&observer);
  }

  {
    // Observer should be called when supported links change.
    EXPECT_CALL(observer, OnArcSupportedLinksChanged);
    instance_->OnSupportedLinksChanged(
        /*added_packages=*/{},
        /*removed_packages=*/{},
        arc::mojom::SupportedLinkChangeSource::kArcSystem);
    testing::Mock::VerifyAndClearExpectations(&observer);
  }

  // Observer should not be called after it's removed.
  instance_->RemoveObserver(&observer);
  instance_->OnIntentFiltersUpdated(/*filters=*/{});
  instance_->OnSupportedLinksChanged(
      /*added_packages=*/{},
      /*removed_packages=*/{},
      arc::mojom::SupportedLinkChangeSource::kArcSystem);
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

// Tests that OnOpenWebApp opens only HTTPS URLs or localhost.
TEST_F(ArcIntentHelperTest, TestOnOpenWebApp) {
  instance_->OnOpenWebApp("http://google.com");
  EXPECT_EQ(GURL(), test_open_url_delegate_->TakeLastOpenedUrl());

  instance_->OnOpenWebApp("http://localhost/");
  EXPECT_EQ(GURL("http://localhost/"),
            test_open_url_delegate_->TakeLastOpenedUrl());

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

// Tests that OnOpenAppWithIntents opens only HTTPS URLs.
TEST_F(ArcIntentHelperTest, TestOnOpenAppWithIntent) {
  auto intent = mojom::LaunchIntent::New();
  intent->action = arc::kIntentActionSend;
  intent->extra_text = "Foo";
  instance_->OnOpenAppWithIntent(GURL("https://www.google.com"),
                                 std::move(intent));
  EXPECT_EQ(GURL("https://www.google.com"),
            test_open_url_delegate_->TakeLastOpenedUrl());
  EXPECT_EQ("Foo", test_open_url_delegate_->TakeLastOpenedIntent()->extra_text);

  instance_->OnOpenAppWithIntent(GURL("http://www.google.com"),
                                 mojom::LaunchIntent::New());
  EXPECT_FALSE(test_open_url_delegate_->TakeLastOpenedUrl().is_valid());
  EXPECT_TRUE(test_open_url_delegate_->TakeLastOpenedIntent().is_null());

  instance_->OnOpenAppWithIntent(GURL("http://localhost:8000/foo"),
                                 mojom::LaunchIntent::New());
  EXPECT_TRUE(test_open_url_delegate_->TakeLastOpenedUrl().is_valid());
  EXPECT_FALSE(test_open_url_delegate_->TakeLastOpenedIntent().is_null());

  instance_->OnOpenAppWithIntent(GURL("chrome://settings"),
                                 mojom::LaunchIntent::New());
  EXPECT_FALSE(test_open_url_delegate_->TakeLastOpenedUrl().is_valid());
  EXPECT_TRUE(test_open_url_delegate_->TakeLastOpenedIntent().is_null());
}

// Tests that AppendStringToIntentHelperPackageName works.
TEST_F(ArcIntentHelperTest, TestAppendStringToIntentHelperPackageName) {
  std::string package_name = kArcIntentHelperPackageName;
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
