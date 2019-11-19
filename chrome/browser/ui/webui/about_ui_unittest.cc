// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/about_ui.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/ui/fake_login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestDataReceiver {
 public:
  TestDataReceiver() = default;
  virtual ~TestDataReceiver() = default;

  bool data_received() const { return data_received_; }

  std::string data() const { return data_; }

  std::string Base64DecodedData() const {
    std::string decoded;
    base::Base64Decode(data_, &decoded);
    return decoded;
  }

  void OnDataReceived(scoped_refptr<base::RefCountedMemory> bytes) {
    data_received_ = true;
    data_ = base::StringPiece(reinterpret_cast<const char*>(bytes->front()),
                              bytes->size())
                .as_string();
  }

 private:
  bool data_received_ = false;
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReceiver);
};

}  // namespace

// Base class for ChromeOS offline terms tests.
class ChromeOSTermsTest : public testing::Test {
 protected:
  ChromeOSTermsTest() {}
  ~ChromeOSTermsTest() override = default;

  void SetUp() override {
    // Create root tmp directory for fake ARC ToS data.
    base::FilePath root_path;
    base::CreateNewTempDirectory(FILE_PATH_LITERAL(""), &root_path);
    ASSERT_TRUE(preinstalled_offline_resources_dir_.Set(root_path));
    arc_tos_dir_ =
        preinstalled_offline_resources_dir_.GetPath().Append("arc_tos");
    ASSERT_TRUE(base::CreateDirectory(arc_tos_dir_));

    tested_html_source_ = std::make_unique<AboutUIHTMLSource>(
        chrome::kChromeUITermsHost, nullptr);
  }

  // Creates directory for the given |locale| that contains terms.html. Writes
  // the |locale| string to the created file.
  bool CreateTermsForLocale(const std::string& locale) {
    base::FilePath dir = arc_tos_dir_.Append(base::ToLowerASCII(locale));
    if (!base::CreateDirectory(dir))
      return false;

    if (base::WriteFile(dir.AppendASCII("terms.html"), locale.c_str(),
                        locale.length()) != static_cast<int>(locale.length())) {
      return false;
    }
    return true;
  }

  // Creates directory for the given |locale| that contains privacy_policy.pdf.
  // Writes the |locale| string to the created file.
  bool CreatePrivacyPolicyForLocale(const std::string& locale) {
    base::FilePath dir = arc_tos_dir_.Append(base::ToLowerASCII(locale));
    if (!base::CreateDirectory(dir))
      return false;

    if (base::WriteFile(dir.AppendASCII("privacy_policy.pdf"), locale.c_str(),
                        locale.length()) != static_cast<int>(locale.length())) {
      return false;
    }
    return true;
  }

  // Sets device region in VPD.
  void SetRegion(const std::string& region) {
    statistics_provider_.SetMachineStatistic(chromeos::system::kRegionKey,
                                             region);
  }

  // Starts data request with the |request_url|.
  void StartRequest(const std::string& request_url,
                    TestDataReceiver* data_receiver) {
    content::WebContents::Getter wc_getter;
    tested_html_source_->StartDataRequest(
        GURL(base::StrCat(
            {"chrome://", chrome::kChromeUITermsHost, "/", request_url})),
        std::move(wc_getter),
        base::BindRepeating(&TestDataReceiver::OnDataReceived,
                            base::Unretained(data_receiver)));
    task_environment_.RunUntilIdle();
  }

  const base::FilePath& PreinstalledOfflineResourcesPath() {
    return preinstalled_offline_resources_dir_.GetPath();
  }

 private:
  base::ScopedTempDir preinstalled_offline_resources_dir_;
  base::FilePath arc_tos_dir_;

  content::BrowserTaskEnvironment task_environment_;

  chromeos::system::ScopedFakeStatisticsProvider statistics_provider_;

  std::unique_ptr<AboutUIHTMLSource> tested_html_source_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSTermsTest);
};

TEST_F(ChromeOSTermsTest, NoData) {
  SetRegion("ca");
  ScopedBrowserLocale browser_locale("en-CA");

  TestDataReceiver terms_data_receiver;
  StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

  EXPECT_TRUE(terms_data_receiver.data_received());
  EXPECT_EQ("", terms_data_receiver.data());

  TestDataReceiver privacy_policy_data_receiver;
  StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_policy_data_receiver);

  EXPECT_TRUE(privacy_policy_data_receiver.data_received());
  EXPECT_EQ("", privacy_policy_data_receiver.data());
}

// Demo mode ARC++ ToS and privacy policy test.
class DemoModeChromeOSTermsTest : public ChromeOSTermsTest {
 protected:
  DemoModeChromeOSTermsTest() = default;
  ~DemoModeChromeOSTermsTest() override = default;

  void SetUp() override {
    ChromeOSTermsTest::SetUp();
    AddDemoModeLocale();

    // Simulate Demo Mode setup.
    chromeos::DBusThreadManager::Initialize();
    fake_login_display_host_ =
        std::make_unique<chromeos::FakeLoginDisplayHost>();
    fake_login_display_host_->StartWizard(
        chromeos::DemoPreferencesScreenView::kScreenId);
    fake_login_display_host_->GetWizardController()
        ->SimulateDemoModeSetupForTesting();
    fake_login_display_host_->GetWizardController()
        ->demo_setup_controller()
        ->SetPreinstalledOfflineResourcesPathForTesting(
            PreinstalledOfflineResourcesPath());
    fake_login_display_host_->GetWizardController()
        ->demo_setup_controller()
        ->TryMountPreinstalledDemoResources(base::DoNothing());
  }

  void TearDown() override { chromeos::DBusThreadManager::Shutdown(); }

  // Adds locales supported by demo mode.
  void AddDemoModeLocale() {
    ASSERT_TRUE(CreateTermsForLocale("apac"));
    ASSERT_TRUE(CreateTermsForLocale("da-DK"));
    ASSERT_TRUE(CreateTermsForLocale("de-de"));
    ASSERT_TRUE(CreateTermsForLocale("emea"));
    ASSERT_TRUE(CreateTermsForLocale("en-CA"));
    ASSERT_TRUE(CreateTermsForLocale("en-GB"));
    ASSERT_TRUE(CreateTermsForLocale("en-IE"));
    ASSERT_TRUE(CreateTermsForLocale("en-US"));
    ASSERT_TRUE(CreateTermsForLocale("eu"));
    ASSERT_TRUE(CreateTermsForLocale("fi-FI"));
    ASSERT_TRUE(CreateTermsForLocale("fr-BE"));
    ASSERT_TRUE(CreateTermsForLocale("fr-CA"));
    ASSERT_TRUE(CreateTermsForLocale("fr-FR"));
    ASSERT_TRUE(CreateTermsForLocale("ko-KR"));
    ASSERT_TRUE(CreateTermsForLocale("nb-NO"));
    ASSERT_TRUE(CreateTermsForLocale("nl-BE"));
    ASSERT_TRUE(CreateTermsForLocale("nl-NL"));
    ASSERT_TRUE(CreateTermsForLocale("sv-SE"));

    ASSERT_TRUE(CreatePrivacyPolicyForLocale("da-DK"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("de-de"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("en-US"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("eu"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("fi-FI"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("fr-BE"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("fr-CA"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("fr-FR"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("ko-KR"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("nb-NO"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("nl-BE"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("nl-NL"));
    ASSERT_TRUE(CreatePrivacyPolicyForLocale("sv-SE"));
  }

 private:
  std::unique_ptr<chromeos::FakeLoginDisplayHost> fake_login_display_host_;

  DISALLOW_COPY_AND_ASSIGN(DemoModeChromeOSTermsTest);
};

TEST_F(DemoModeChromeOSTermsTest, TermsSimpleRegion) {
  SetRegion("ca");
  for (const char* locale : {"en-CA", "fr-CA"}) {
    ScopedBrowserLocale browser_locale(locale);

    TestDataReceiver data_receiver;
    StartRequest(chrome::kArcTermsURLPath, &data_receiver);

    EXPECT_TRUE(data_receiver.data_received());
    EXPECT_EQ(locale, data_receiver.data());
  }
}

TEST_F(DemoModeChromeOSTermsTest, ComplexRegion) {
  const std::string kLocale = "en-CA";
  ScopedBrowserLocale browser_locale(kLocale);
  for (const char* region :
       {"ca.hybridansi", "ca.ansi", "ca.multix", "ca.fr"}) {
    SetRegion(region);

    TestDataReceiver terms_data_receiver;
    StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

    EXPECT_TRUE(terms_data_receiver.data_received());
    EXPECT_EQ(kLocale, terms_data_receiver.data());

    TestDataReceiver privacy_data_receiver;
    StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

    // Privacy policy for en-CA defaults to en-US.
    EXPECT_TRUE(privacy_data_receiver.data_received());
    EXPECT_EQ("en-US", privacy_data_receiver.Base64DecodedData());
  }
}

TEST_F(DemoModeChromeOSTermsTest, NotCaseSensitive) {
  SetRegion("CA");
  for (const char* locale : {"EN-CA", "en-CA", "EN-ca"}) {
    ScopedBrowserLocale browser_locale(locale);

    TestDataReceiver terms_data_receiver;
    StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

    EXPECT_TRUE(terms_data_receiver.data_received());
    EXPECT_EQ("en-CA", terms_data_receiver.data());

    TestDataReceiver privacy_data_receiver;
    StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

    // Privacy policy for en-CA defaults to en-US.
    EXPECT_TRUE(privacy_data_receiver.data_received());
    EXPECT_EQ("en-US", privacy_data_receiver.Base64DecodedData());
  }
}

TEST_F(DemoModeChromeOSTermsTest, DefaultsForEuRegion) {
  const std::string kLocale = "pl-PL";
  ScopedBrowserLocale browser_locale(kLocale);
  SetRegion("pl");

  TestDataReceiver terms_data_receiver;
  StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

  EXPECT_TRUE(terms_data_receiver.data_received());
  EXPECT_EQ("eu", terms_data_receiver.data());

  TestDataReceiver privacy_data_receiver;
  StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

  EXPECT_TRUE(privacy_data_receiver.data_received());
  EXPECT_EQ("eu", privacy_data_receiver.Base64DecodedData());
}

TEST_F(DemoModeChromeOSTermsTest, DefaultsForEmeaRegion) {
  const std::string kLocale = "fr-CH";
  ScopedBrowserLocale browser_locale(kLocale);
  SetRegion("ch");

  TestDataReceiver terms_data_receiver;
  StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

  EXPECT_TRUE(terms_data_receiver.data_received());
  EXPECT_EQ("emea", terms_data_receiver.data());

  TestDataReceiver privacy_data_receiver;
  StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

  // Privacy policy for EMEA defaults to en-US.
  EXPECT_TRUE(privacy_data_receiver.data_received());
  EXPECT_EQ("en-US", privacy_data_receiver.Base64DecodedData());
}

TEST_F(DemoModeChromeOSTermsTest, DefaultsForApacRegion) {
  const std::string kLocale = "en-PH";
  ScopedBrowserLocale browser_locale(kLocale);
  SetRegion("ph");

  TestDataReceiver terms_data_receiver;
  StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

  EXPECT_TRUE(terms_data_receiver.data_received());
  EXPECT_EQ("apac", terms_data_receiver.data());

  TestDataReceiver privacy_data_receiver;
  StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

  // Privacy policy for APAC defaults to en-US.
  EXPECT_TRUE(privacy_data_receiver.data_received());
  EXPECT_EQ("en-US", privacy_data_receiver.Base64DecodedData());
}

TEST_F(DemoModeChromeOSTermsTest, DefaultsForAmericasRegion) {
  const std::string kLocale = "en-MX";
  ScopedBrowserLocale browser_locale(kLocale);
  SetRegion("mx");

  // Both ToS and privacy policy default to en-US for AMERICAS.
  TestDataReceiver terms_data_receiver;
  StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

  EXPECT_TRUE(terms_data_receiver.data_received());
  EXPECT_EQ("en-US", terms_data_receiver.data());

  TestDataReceiver privacy_data_receiver;
  StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

  EXPECT_TRUE(privacy_data_receiver.data_received());
  EXPECT_EQ("en-US", privacy_data_receiver.Base64DecodedData());
}

TEST_F(DemoModeChromeOSTermsTest, DefaultsToEnUs) {
  const std::string kLocale = "en-SA";
  ScopedBrowserLocale browser_locale(kLocale);
  SetRegion("sa");

  TestDataReceiver terms_data_receiver;
  StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

  EXPECT_TRUE(terms_data_receiver.data_received());
  EXPECT_EQ("en-US", terms_data_receiver.data());

  TestDataReceiver privacy_data_receiver;
  StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

  EXPECT_TRUE(privacy_data_receiver.data_received());
  EXPECT_EQ("en-US", privacy_data_receiver.Base64DecodedData());
}

TEST_F(DemoModeChromeOSTermsTest, NoLangCountryCombination) {
  SetRegion("be");
  {
    const std::string kLocale = "nl-BE";
    ScopedBrowserLocale browser_locale(kLocale);

    TestDataReceiver terms_data_receiver;
    StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

    EXPECT_TRUE(terms_data_receiver.data_received());
    EXPECT_EQ(kLocale, terms_data_receiver.data());

    TestDataReceiver privacy_data_receiver;
    StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

    EXPECT_TRUE(privacy_data_receiver.data_received());
    EXPECT_EQ(kLocale, privacy_data_receiver.Base64DecodedData());
  }
  {
    const std::string kLocale = "de-BE";
    ScopedBrowserLocale browser_locale(kLocale);

    TestDataReceiver terms_data_receiver;
    StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

    // No language - country combination - defaults to region.
    EXPECT_TRUE(terms_data_receiver.data_received());
    EXPECT_EQ("eu", terms_data_receiver.data());

    TestDataReceiver privacy_data_receiver;
    StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

    EXPECT_TRUE(privacy_data_receiver.data_received());
    EXPECT_EQ("eu", privacy_data_receiver.Base64DecodedData());
  }
}

TEST_F(DemoModeChromeOSTermsTest, InvalidRegion) {
  const std::string kLocale = "da-DK";
  ScopedBrowserLocale browser_locale(kLocale);
  for (const char* region : {"", " ", ".", "..", "-", "xyz"}) {
    SetRegion(region);
    TestDataReceiver terms_data_receiver;
    StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

    EXPECT_TRUE(terms_data_receiver.data_received());
    EXPECT_EQ("en-US", terms_data_receiver.data());

    TestDataReceiver privacy_data_receiver;
    StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

    EXPECT_TRUE(privacy_data_receiver.data_received());
    EXPECT_EQ("en-US", privacy_data_receiver.Base64DecodedData());
  }
}

TEST_F(DemoModeChromeOSTermsTest, InvalidLocale) {
  SetRegion("se");
  for (const char* locale : {"", " ", ".", "-", "-sv"}) {
    ScopedBrowserLocale browser_locale(locale);

    TestDataReceiver terms_data_receiver;
    StartRequest(chrome::kArcTermsURLPath, &terms_data_receiver);

    EXPECT_TRUE(terms_data_receiver.data_received());
    EXPECT_EQ("eu", terms_data_receiver.data());

    TestDataReceiver privacy_data_receiver;
    StartRequest(chrome::kArcPrivacyPolicyURLPath, &privacy_data_receiver);

    EXPECT_TRUE(privacy_data_receiver.data_received());
    EXPECT_EQ("eu", privacy_data_receiver.Base64DecodedData());
  }
}
