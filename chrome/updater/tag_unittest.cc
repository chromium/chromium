// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/tag.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/updater/test/unit_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using updater::tagging::AppArgs;
using updater::tagging::ErrorCode;
using updater::tagging::NeedsAdmin;
using updater::tagging::RuntimeModeArgs;
using updater::tagging::TagArgs;

// Builder pattern helper to construct the TagArgs struct.
class TagArgsBuilder {
 public:
  TagArgs Build() { return std::move(inner_); }

  TagArgsBuilder& WithBundleName(const std::string& bundle_name) {
    this->inner_.bundle_name = bundle_name;
    return *this;
  }
  TagArgsBuilder& WithInstallationId(const std::string& installation_id) {
    this->inner_.installation_id = installation_id;
    return *this;
  }
  TagArgsBuilder& WithBrandCode(const std::string& brand_code) {
    this->inner_.brand_code = brand_code;
    return *this;
  }
  TagArgsBuilder& WithClientId(const std::string& client_id) {
    this->inner_.client_id = client_id;
    return *this;
  }
  TagArgsBuilder& WithExperimentLabels(const std::string& experiment_labels) {
    this->inner_.experiment_labels = experiment_labels;
    return *this;
  }
  TagArgsBuilder& WithReferralId(const std::string& referral_id) {
    this->inner_.referral_id = referral_id;
    return *this;
  }
  TagArgsBuilder& WithLanguage(const std::string& language) {
    this->inner_.language = language;
    return *this;
  }
  TagArgsBuilder& WithFlighting(bool flighting) {
    this->inner_.flighting = flighting;
    return *this;
  }
  TagArgsBuilder& WithBrowserType(TagArgs::BrowserType browser_type) {
    this->inner_.browser_type = browser_type;
    return *this;
  }
  TagArgsBuilder& WithUsageStatsEnable(bool usage_stats_enable) {
    this->inner_.usage_stats_enable = usage_stats_enable;
    return *this;
  }
  TagArgsBuilder& WithApp(AppArgs app) {
    this->inner_.apps.push_back(std::move(app));
    return *this;
  }
  TagArgsBuilder& WithRuntimeMode(RuntimeModeArgs runtime_mode) {
    this->inner_.runtime_mode = runtime_mode;
    return *this;
  }
  TagArgsBuilder& WithEnrollmentToken(const std::string& enrollment_token) {
    this->inner_.enrollment_token = enrollment_token;
    return *this;
  }

 private:
  TagArgs inner_;
};

// Builder pattern helper to construct the AppArgs struct.
class AppArgsBuilder {
 public:
  explicit AppArgsBuilder(const std::string& app_id) : inner_(app_id) {}

  AppArgs Build() { return std::move(inner_); }

  AppArgsBuilder& WithAppName(const std::string& app_name) {
    this->inner_.app_name = app_name;
    return *this;
  }
  AppArgsBuilder& WithNeedsAdmin(NeedsAdmin needs_admin) {
    this->inner_.needs_admin = needs_admin;
    return *this;
  }
  AppArgsBuilder& WithAp(const std::string& ap) {
    this->inner_.ap = ap;
    return *this;
  }
  AppArgsBuilder& WithEncodedInstallerData(
      const std::string& encoded_installer_data) {
    this->inner_.encoded_installer_data = encoded_installer_data;
    return *this;
  }
  AppArgsBuilder& WithInstallDataIndex(const std::string& install_data_index) {
    this->inner_.install_data_index = install_data_index;
    return *this;
  }
  AppArgsBuilder& WithExperimentLabels(const std::string& experiment_labels) {
    this->inner_.experiment_labels = experiment_labels;
    return *this;
  }
  AppArgsBuilder& WithUntrustedData(const std::string& untrusted_data) {
    this->inner_.untrusted_data = untrusted_data;
    return *this;
  }

 private:
  AppArgs inner_;
};

// Builder pattern helper to construct the RuntimeModeArgs struct.
class RuntimeModeArgsBuilder {
 public:
  RuntimeModeArgsBuilder() = default;

  RuntimeModeArgs Build() { return std::move(inner_); }

  RuntimeModeArgsBuilder& WithNeedsAdmin(NeedsAdmin needs_admin) {
    this->inner_.needs_admin = needs_admin;
    return *this;
  }

 private:
  RuntimeModeArgs inner_;
};

void VerifyTagParseSuccess(
    std::string_view tag,
    std::optional<std::string_view> app_installer_data_args,
    const TagArgs& expected) {
  TagArgs actual;
  ASSERT_EQ(ErrorCode::kSuccess, Parse(tag, app_installer_data_args, actual));

  updater::test::ExpectTagArgsEqual(actual, expected);
}

void VerifyTagParseFail(std::string_view tag,
                        std::optional<std::string_view> app_installer_data_args,
                        ErrorCode expected) {
  TagArgs args;
  ASSERT_EQ(expected, Parse(tag, app_installer_data_args, args));
}

}  // namespace

namespace updater {

using tagging::AppArgs;
using tagging::ErrorCode;
using tagging::Parse;
using tagging::TagArgs;

TEST(TagParserTest, InvalidValueNameIsSupersetOfValidName) {
  VerifyTagParseFail(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname1=Hello",
      std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, AppNameSpaceForValue) {
  VerifyTagParseFail(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname= ",
      std::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, AppNameEncodedSpaceForValue) {
  VerifyTagParseFail(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=%20",
      std::nullopt, ErrorCode::kApp_AppNameCannotBeWhitespace);
}

TEST(TagParserTest, AppNameValid) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=Test",
      std::nullopt,
      TagArgsBuilder()
          .WithBundleName("Test")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("Test")
                       .Build())
          .Build());
}

// This must work because the enterprise MSI code assumes spaces are allowed.
TEST(TagParserTest, AppNameWithSpace) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=Test App",
      std::nullopt,
      TagArgsBuilder()
          .WithBundleName("Test App")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("Test App")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameWithSpaceAtEnd) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname= T Ap p ",
      std::nullopt,
      TagArgsBuilder()
          .WithBundleName("T Ap p")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("T Ap p")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameWithEncodedSpacesAtEnd) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=%20T%20Ap%20p%20",
      std::nullopt,
      TagArgsBuilder()
          .WithBundleName("T Ap p")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("T Ap p")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameWithMultipleSpaces) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname= T Ap p",
      std::nullopt,
      TagArgsBuilder()
          .WithBundleName("T Ap p")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("T Ap p")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameUnicode) {
  std::string non_ascii_name = "रहा";
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=%E0%A4%B0%E0%A4%B9%E0%A4%BE",
      std::nullopt,
      TagArgsBuilder()
          .WithBundleName(non_ascii_name)
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName(non_ascii_name)
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameUnicode2) {
  std::string non_ascii_name = "स्थापित कर रहा है।";
  std::string escaped(
      "%E0%A4%B8%E0%A5%8D%E0%A4%A5%E0%A4%BE%E0%A4%AA%E0%A4%BF"
      "%E0%A4%A4%20%E0%A4%95%E0%A4%B0%20%E0%A4%B0%E0%A4%B9%E0"
      "%A4%BE%20%E0%A4%B9%E0%A5%88%E0%A5%A4");

  std::stringstream tag;
  tag << "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&";
  tag << "appname=" << escaped;
  VerifyTagParseSuccess(
      tag.str(), std::nullopt,
      TagArgsBuilder()
          .WithBundleName(non_ascii_name)
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName(non_ascii_name)
                       .Build())
          .Build());
}

TEST(TagParserTest, AppIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B", std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .Build());
}

TEST(TagParserTest, AppIdNotASCII) {
  VerifyTagParseFail("appguid=रहा", std::nullopt,
                     ErrorCode::kApp_AppIdIsNotValid);
}

// Most tests here do not reflect this, but appids can be non-GUID ASCII
// strings.
TEST(TagParserTest, AppIdNotAGuid) {
  VerifyTagParseSuccess(
      "appguid=non-guid-id", std::nullopt,
      TagArgsBuilder().WithApp(AppArgsBuilder("non-guid-id").Build()).Build());
}

TEST(TagParserTest, AppIdCaseInsensitive) {
  VerifyTagParseSuccess(
      "appguid=ShouldBeCaseInsensitive", std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("shouldbecaseinsensitive").Build())
          .Build());
}

TEST(TagParserTest, NoRuntimeModeOrAppOnlyNeedsAdminValue) {
  VerifyTagParseFail("needsadmin=true", std::nullopt,
                     ErrorCode::kApp_AppIdNotSpecified);
}

TEST(TagParserTest, NeedsAdminInvalid) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=Hello",
      std::nullopt, ErrorCode::kApp_NeedsAdminValueIsInvalid);
}

TEST(TagParserTest, NeedsAdminSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin= ",
      std::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, NeedsAdminTrueUpperCaseT) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=True",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithNeedsAdmin(NeedsAdmin::kYes)
                       .Build())
          .Build());
}

TEST(TagParserTest, NeedsAdminTrueLowerCaseT) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=true",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithNeedsAdmin(NeedsAdmin::kYes)
                       .Build())
          .Build());
}

TEST(TagParserTest, NeedsFalseUpperCaseF) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=False",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithNeedsAdmin(NeedsAdmin::kNo)
                       .Build())
          .Build());
}

TEST(TagParserTest, NeedsAdminFalseLowerCaseF) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=false",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithNeedsAdmin(NeedsAdmin::kNo)
                       .Build())
          .Build());
}

//
// Test the handling of the contents of the extra arguments.
//

TEST(TagParserTest, AssignmentOnly) {
  VerifyTagParseFail("=", std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, ExtraAssignment1) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1=",
      std::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, ExtraAssignment2) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "=usagestats=1",
      std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, ExtraAssignment3) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1&=",
      std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, ExtraAssignment4) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "=&usagestats=1",
      std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, ValueWithoutName) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "=hello",
      std::nullopt, ErrorCode::kUnrecognizedName);
}

// Also tests ending extra arguments with '='.
TEST(TagParserTest, NameWithoutValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=",
      std::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, NameWithoutValueBeforeNextArgument) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=&client=hello",
      std::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, NameWithoutArgumentSeparatorAfterIntValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1client=hello",
      std::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, NameWithoutArgumentSeparatorAfterStringValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=yesclient=hello",
      std::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, TagHasDoubleAmpersand) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1&&client=hello",
      {"appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
       "installerdata=foobar"},
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithEncodedInstallerData("foobar")
                       .Build())
          .WithUsageStatsEnable(true)
          .WithClientId("hello")
          .Build());
}

TEST(TagParserTest, TagAmpersandOnly) {
  VerifyTagParseSuccess("&", std::nullopt, TagArgsBuilder().Build());
}

TEST(TagParserTest, TagBeginsInAmpersand) {
  VerifyTagParseSuccess(
      "&appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, TagEndsInAmpersand) {
  VerifyTagParseSuccess(
      "&appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1&",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhitespaceOnly) {
  for (const auto& whitespace : {"", " ", "\t", "\r", "\n", "\r\n"}) {
    VerifyTagParseSuccess(whitespace, std::nullopt, TagArgsBuilder().Build());
  }
}

//
// Test the parsing of the extra command and its arguments into a string.
//

TEST(TagParserTest, OneValidAttribute) {
  VerifyTagParseSuccess(
      "&appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, TwoValidAttributes) {
  VerifyTagParseSuccess(
      "&appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1&client=hello",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .WithClientId("hello")
          .Build());
}

TEST(TagParserTest, TagHasSwitchInTheMiddle) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1/other_value=9",
      std::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, TagHasDoubleQuoteInTheMiddle) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1\"/other_value=9",
      std::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, TagHasDoubleQuoteInTheMiddleAndNoForwardSlash) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1\"other_value=9",
      std::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, TagHasSpaceAndForwardSlashBeforeQuote) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1 /other_value=9",
      std::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, TagHasForwardSlashBeforeQuote) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1/other_value=9",
      std::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, AttributeSpecifiedTwice) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1\" \"client=10",
      std::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, WhiteSpaceBeforeArgs1) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      " usagestats=1",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhiteSpaceBeforeArgs2) {
  VerifyTagParseSuccess(
      "\tappguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhiteSpaceBeforeArgs3) {
  VerifyTagParseSuccess(
      "\rappguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhiteSpaceBeforeArgs4) {
  VerifyTagParseSuccess(
      "\nappguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B"
      "&usagestats=1",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhiteSpaceBeforeArgs5) {
  VerifyTagParseSuccess("\r\nusagestats=1", std::nullopt,
                        TagArgsBuilder().WithUsageStatsEnable(true).Build());
}

TEST(TagParserTest, ForwardSlash1) {
  VerifyTagParseFail("/", std::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, ForwardSlash2) {
  VerifyTagParseFail("/ appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B",
                     std::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, BackwardSlash1) {
  VerifyTagParseFail("\\", std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, BackwardSlash2) {
  VerifyTagParseFail("\\appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B",
                     std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, BackwardSlash3) {
  VerifyTagParseFail("\\ appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B",
                     std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, AppArgsMustHaveValue) {
  for (const auto& tag :
       {"appguid", "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&ap",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&experiments",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&appname",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&needsadmin",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&installdataindex",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&untrusteddata"}) {
    VerifyTagParseFail(tag, std::nullopt, ErrorCode::kAttributeMustHaveValue);
  }
}

//
// Test specific extra commands.
//

TEST(TagParserTest, UsageStatsOutsideExtraCommand) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "/usagestats",
      std::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, UsageStatsOn) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, UsageStatsOff) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=0",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(false)
          .Build());
}

TEST(TagParserTest, UsageStatsNone) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=2",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .Build());
}

TEST(TagParserTest, UsageStatsInvalidPositiveValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=3",
      std::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, UsageStatsInvalidNegativeValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=-1",
      std::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, UsageStatsValueIsString) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=true",
      std::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, BundleNameValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "bundlename=Google%20Bundle",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithBundleName("Google Bundle")

          .Build());
}

TEST(TagParserTest, BundleNameSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "bundlename= ",
      std::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, BundleNameEncodedSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "bundlename=%20",
      std::nullopt, ErrorCode::kGlobal_BundleNameCannotBeWhitespace);
}

TEST(TagParserTest, BundleNameNotPresentButAppNameIs) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=Google%20Chrome",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithAppName("Google Chrome")
                       .Build())
          .WithBundleName("Google Chrome")
          .Build());
}
TEST(TagParserTest, BundleNameNorAppNamePresent) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&", std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .Build());
}

TEST(TagParserTest, BundleNameNotPresentAndNoApp) {
  VerifyTagParseSuccess(
      "browser=0", std::nullopt,
      TagArgsBuilder().WithBrowserType(TagArgs::BrowserType::kUnknown).Build());
}

TEST(TagParserTest, InstallationIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "iid=98CEC468-9429-4984-AEDE-4F53C6A14869",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithInstallationId("98CEC468-9429-4984-AEDE-4F53C6A14869")
          .Build());
}

TEST(TagParserTest, InstallationIdContainsNonASCII) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "iid=रहा",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithInstallationId("रहा")
          .Build());
}

TEST(TagParserTest, BrandCodeValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "brand=GOOG",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithBrandCode("GOOG")
          .Build());
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "brand=GOOGLE",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithBrandCode("GOOGLE")
          .Build());
}

TEST(TagParserTest, ClientIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "client=some_partner",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithClientId("some_partner")
          .Build());
}

TEST(TagParserTest, UpdaterExperimentIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "omahaexperiments=experiment%3DgroupA%7Cexpir",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithExperimentLabels("experiment=groupA|expir")
          .Build());
}

TEST(TagParserTest, UpdaterExperimentIdSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "omahaexperiments= ",
      std::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, UpdaterExperimentIdEncodedSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "omahaexperiments=%20",
      std::nullopt, ErrorCode::kGlobal_ExperimentLabelsCannotBeWhitespace);
}

TEST(TagParserTest, AppExperimentIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "experiments=experiment%3DgroupA%7Cexpir",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithExperimentLabels("experiment=groupA|expir")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppExperimentIdSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "experiments= ",
      std::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, AppExperimentIdEncodedSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "experiments=%20",
      std::nullopt, ErrorCode::kApp_ExperimentLabelsCannotBeWhitespace);
}

TEST(TagParserTest, ReferralIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "referral=ABCD123",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithReferralId("ABCD123")
          .Build());
}

TEST(TagParserTest, ApValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "ap=developer",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithAp("developer")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppInstallerDataArgsValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&",
      {"appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
       "installerdata=%E0%A4foobar"},
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithEncodedInstallerData("%E0%A4foobar")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppInstallerDataArgsInvalidAppId) {
  VerifyTagParseFail("appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&",
                     {"appguid=E135384F-85A2-4328-B07D-2CF70313D505&"
                      "installerdata=%E0%A4foobar"},
                     ErrorCode::kAppInstallerData_AppIdNotFound);
}

TEST(TagParserTest, AppInstallerDataArgsInvalidAttribute) {
  VerifyTagParseFail("appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&",
                     {"appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
                      "needsadmin=true&"},
                     ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, InstallerDataNotAllowedInTag) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=TestApp2&"
      "needsadmin=true&"
      "installerdata=Hello%20World",
      std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, InstallDataIndexValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "installdataindex=foobar",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithInstallDataIndex("foobar")
                       .Build())
          .Build());
}

TEST(TagParserTest, BrowserTypeValid) {
  std::tuple<std::string_view, TagArgs::BrowserType>
      pairs[base::to_underlying(TagArgs::BrowserType::kMax)] = {
          {"0", TagArgs::BrowserType::kUnknown},
          {"1", TagArgs::BrowserType::kDefault},
          {"2", TagArgs::BrowserType::kInternetExplorer},
          {"3", TagArgs::BrowserType::kFirefox},
          {"4", TagArgs::BrowserType::kChrome},
      };

  for (const auto& [browser, browser_type] : pairs) {
    std::stringstream tag;
    tag << "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&";
    tag << "browser=" << browser;
    VerifyTagParseSuccess(
        tag.str(), std::nullopt,
        TagArgsBuilder()
            .WithApp(
                AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
            .WithBrowserType(browser_type)
            .Build());
  }
}

TEST(TagParserTest, BrowserTypeInvalid) {
  EXPECT_EQ(5, int(TagArgs::BrowserType::kMax))
      << "Browser type may have been added. Update the BrowserTypeValid test "
         "and change browser values in tag strings below.";

  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "browser=5",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithBrowserType(TagArgs::BrowserType::kUnknown)
          .Build());

  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "browser=9",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithBrowserType(TagArgs::BrowserType::kUnknown)
          .Build());
}

TEST(TagParserTest, ValidLang) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "lang=en",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithLanguage("en")
          .Build());
}

// Language must be passed even if not supported. See http://b/1336966.
TEST(TagParserTest, UnsupportedLang) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "lang=foobar",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithLanguage("foobar")
          .Build());
}

TEST(TagParserTest, AppNameSpecifiedTwice) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=TestApp&"
      "appname=TestApp2&",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B")
                       .WithAppName("TestApp2")
                       .Build())
          .WithBundleName("TestApp2")
          .Build());
}

TEST(TagParserTest, CaseInsensitiveAttributeNames) {
  VerifyTagParseSuccess(
      "APPguID=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "APPNAME=TestApp&",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B")
                       .WithAppName("TestApp")
                       .Build())
          .WithBundleName("TestApp")
          .Build());
}

TEST(TagParserTest, BracesEncoding) {
  VerifyTagParseSuccess(
      "appguid=%7B8617EE50-F91C-4DC1-B937-0969EEF59B0B%7D&"
      "appname=TestApp&",
      std::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}")
                       .WithAppName("TestApp")
                       .Build())
          .WithBundleName("TestApp")
          .Build());
}

//
// Test multiple applications in the extra arguments
//
TEST(TagParserTestMultipleEntries, TestNotStartingWithAppId) {
  VerifyTagParseFail(
      "appname=TestApp&"
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=TestApp&"
      "appname=false&"
      "iid=98CEC468-9429-4984-AEDE-4F53C6A14869&"
      "ap=test_ap&"
      "usagestats=1&"
      "browser=2&",
      std::nullopt, ErrorCode::kApp_AppIdNotSpecified);
}

// This also tests that the last occurrence of a global extra arg is the one
// that is saved.
TEST(TagParserTestMultipleEntries, ThreeApplications) {
  VerifyTagParseSuccess(
      "etoken=5d086552-4514-4dfb-8a3e-337024ec35ac&"
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=TestApp&"
      "needsadmin=false&"
      "iid=98CEC468-9429-4984-AEDE-4F53C6A14869&"
      "ap=test_ap&"
      "usagestats=1&"
      "browser=2&"
      "brand=GOOG&"
      "client=_some_client&"
      "experiments=_experiment_a&"
      "untrusteddata=ABCDEFG&"
      "referral=A123456789&"
      "appguid=5E46DE36-737D-4271-91C1-C062F9FE21D9&"
      "appname=TestApp2&"
      "needsadmin=true&"
      "experiments=_experiment_b&"
      "untrusteddata=1234567&"
      "iid=98CEC468-9429-4984-AEDE-4F53C6A14869&"
      "ap=test_ap2&"
      "usagestats=0&"
      "browser=3&"
      "brand=g00g&"
      "client=_different_client&"
      "appguid=5F46DE36-737D-4271-91C1-C062F9FE21D9&"
      "appname=TestApp3&"
      "needsadmin=prefers&",
      {"appguid=5F46DE36-737D-4271-91C1-C062F9FE21D9&"
       "installerdata=installerdata_app3&"
       "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
       "installerdata=installerdata_app1"},
      TagArgsBuilder()
          .WithEnrollmentToken("5d086552-4514-4dfb-8a3e-337024ec35ac")
          .WithBundleName("TestApp")
          .WithInstallationId("98CEC468-9429-4984-AEDE-4F53C6A14869")
          .WithBrandCode("g00g")
          .WithClientId("_different_client")
          .WithReferralId("A123456789")
          .WithBrowserType(TagArgs::BrowserType::kFirefox)
          .WithUsageStatsEnable(false)
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithAppName("TestApp")
                       .WithNeedsAdmin(NeedsAdmin::kNo)
                       .WithAp("test_ap")
                       .WithEncodedInstallerData("installerdata_app1")
                       .WithExperimentLabels("_experiment_a")
                       .WithUntrustedData("A=3&Message=Hello,%20World!")
                       .Build())
          .WithApp(AppArgsBuilder("5e46de36-737d-4271-91c1-c062f9fe21d9")
                       .WithAppName("TestApp2")
                       .WithNeedsAdmin(NeedsAdmin::kYes)
                       .WithAp("test_ap2")
                       .WithExperimentLabels("_experiment_b")
                       .WithUntrustedData("X=5")
                       .Build())
          .WithApp(AppArgsBuilder("5f46de36-737d-4271-91c1-c062f9fe21d9")
                       .WithAppName("TestApp3")
                       .WithEncodedInstallerData("installerdata_app3")
                       .WithNeedsAdmin(NeedsAdmin::kPrefers)
                       .Build())
          .Build());
}

TEST(TagParserTest, RuntimeModeBeforeApp) {
  VerifyTagParseFail(
      "runtime=true&appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname1=Hello",
      std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, RuntimeModeAfterApp) {
  VerifyTagParseFail(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&appname1=Hello&runtime="
      "true",
      std::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, RuntimeModeIncorrectValue) {
  VerifyTagParseFail("runtime=foo", std::nullopt,
                     ErrorCode::kGlobal_RuntimeModeValueIsInvalid);
}

TEST(TagParserTest, RuntimeModeIncorrectNeedsAdminValue) {
  VerifyTagParseFail("runtime=true&needsadmin=foo", std::nullopt,
                     ErrorCode::kRuntimeMode_NeedsAdminValueIsInvalid);
}

TEST(TagParserTest, RuntimeModeValid) {
  VerifyTagParseSuccess("runtime=true", std::nullopt,
                        TagArgsBuilder()
                            .WithRuntimeMode(RuntimeModeArgsBuilder().Build())
                            .Build());
}

TEST(TagParserTest, RuntimeModeValidSystem) {
  VerifyTagParseSuccess(
      "runtime=true&needsadmin=true", std::nullopt,
      TagArgsBuilder()
          .WithRuntimeMode(
              RuntimeModeArgsBuilder().WithNeedsAdmin(NeedsAdmin::kYes).Build())
          .Build());
}

TEST(TagParserTest, RuntimeModeValidUser) {
  VerifyTagParseSuccess(
      "runtime=true&needsadmin=false", std::nullopt,
      TagArgsBuilder()
          .WithRuntimeMode(
              RuntimeModeArgsBuilder().WithNeedsAdmin(NeedsAdmin::kNo).Build())
          .Build());
}

TEST(TagParserTest, EnrollmentTokenBeforeApp) {
  VerifyTagParseSuccess(
      "etoken=5d086552-4514-4dfb-8a3e-337024ec35ac&"
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=Hello",
      std::nullopt,
      TagArgsBuilder()
          .WithEnrollmentToken("5d086552-4514-4dfb-8a3e-337024ec35ac")
          .WithBundleName("Hello")
          .WithApp(AppArgsBuilder("D0324988-DA8A-49e5-BCE5-925FCD04EAB7")
                       .WithAppName("Hello")
                       .Build())
          .Build());
}

TEST(TagParserTest, EnrollmentTokenAfterApp) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=Hello&"
      "etoken=5d086552-4514-4dfb-8a3e-337024ec35ac",
      std::nullopt,
      TagArgsBuilder()
          .WithEnrollmentToken("5d086552-4514-4dfb-8a3e-337024ec35ac")
          .WithBundleName("Hello")
          .WithApp(AppArgsBuilder("D0324988-DA8A-49e5-BCE5-925FCD04EAB7")
                       .WithAppName("Hello")
                       .Build())
          .Build());
}

TEST(TagParserTest, EnrollmentTokenInvalidValue) {
  VerifyTagParseFail("etoken=5d086552-4514-____-8a3e-337024ec35ac",
                     std::nullopt,
                     ErrorCode::kGlobal_EnrollmentTokenValueIsInvalid);
}

TEST(TagParserTest, EnrollmentTokenValid) {
  VerifyTagParseSuccess(
      "etoken=5d086552-4514-4dfb-8a3e-337024ec35ac", std::nullopt,
      TagArgsBuilder()
          .WithEnrollmentToken("5d086552-4514-4dfb-8a3e-337024ec35ac")
          .Build());
}

TEST(TagExtractorTest, AdvanceIt) {
  const std::vector<uint8_t> empty_binary;
  ASSERT_TRUE(tagging::internal::AdvanceIt(empty_binary.begin(), 0,
                                           empty_binary.end()) ==
              empty_binary.end());

  const std::vector<uint8_t> binary(5);
  std::vector<uint8_t>::const_iterator it = binary.begin();
  ASSERT_TRUE(tagging::internal::AdvanceIt(it, 0, binary.end()) == it);
  ASSERT_TRUE(tagging::internal::AdvanceIt(it, 4, binary.end()) == (it + 4));
  ASSERT_TRUE(tagging::internal::AdvanceIt(it, 5, binary.end()) ==
              binary.end());
  ASSERT_TRUE(tagging::internal::AdvanceIt(it, 6, binary.end()) ==
              binary.end());
}

TEST(TagExtractorTest, CheckRange) {
  const std::vector<uint8_t> empty_binary;
  ASSERT_FALSE(
      tagging::internal::CheckRange(empty_binary.end(), 1, empty_binary.end()));

  const std::vector<uint8_t> binary(5);

  std::vector<uint8_t>::const_iterator it = binary.begin();
  ASSERT_FALSE(tagging::internal::CheckRange(it, 0, binary.end()));
  ASSERT_TRUE(tagging::internal::CheckRange(it, 1, binary.end()));
  ASSERT_TRUE(tagging::internal::CheckRange(it, 5, binary.end()));
  ASSERT_FALSE(tagging::internal::CheckRange(it, 6, binary.end()));

  it = binary.begin() + 2;
  ASSERT_TRUE(tagging::internal::CheckRange(it, 3, binary.end()));
  ASSERT_FALSE(tagging::internal::CheckRange(it, 4, binary.end()));

  it = binary.begin() + 5;
  ASSERT_FALSE(tagging::internal::CheckRange(it, 0, binary.end()));
  ASSERT_FALSE(tagging::internal::CheckRange(it, 1, binary.end()));
}

TEST(ExeTagTest, FileNotFound) {
  ASSERT_TRUE(
      tagging::BinaryReadTagString(test::GetTestFilePath("FileNotFound.exe"))
          .empty());
}

TEST(ExeTagTest, UntaggedExe) {
  ASSERT_TRUE(tagging::BinaryReadTagString(test::GetTestFilePath("signed.exe"))
                  .empty());
}

TEST(ExeTagTest, TaggedExeEncodeUtf8) {
  ASSERT_EQ(tagging::BinaryReadTagString(
                test::GetTestFilePath("tagged_encode_utf8.exe")),
            "TestTag123");
}

struct ExeTagTestExeWriteTagTestCase {
  const std::string exe_file_name;
  const std::string tag_string;
  const bool expected_success;
};

class ExeTagTestExeWriteTagTest
    : public ::testing::TestWithParam<ExeTagTestExeWriteTagTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ExeTagTestExeWriteTagTestCases,
    ExeTagTestExeWriteTagTest,
    ::testing::ValuesIn(std::vector<ExeTagTestExeWriteTagTestCase>{
        // single tag parameter.
        {"signed.exe", "brand=QAQA", true},

        // single tag parameter ending in an ampersand.
        {"signed.exe", "brand=QAQA&", true},

        // multiple tag parameters.
        {"signed.exe",
         "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&iid={2D8C18E9-8D3A-"
         "4EFC-"
         "6D61-AE23E3530EA2}&lang=en&browser=4&usagestats=0&appname=Google%"
         "20Chrome&needsadmin=prefers&brand=CHMB&installdataindex="
         "defaultbrowser",
         true},

        // already tagged.
        {"tagged_encode_utf8.exe", "brand=QAQA", true},

        // empty tag string.
        {"signed.exe", "", true},

        // unknown tag argument `unknowntagarg`.
        {"signed.exe",
         "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&iid={2D8C18E9-8D3A-"
         "4EFC-"
         "6D61-AE23E3530EA2}&unknowntagarg=foo",
         false},
    }));

TEST_P(ExeTagTestExeWriteTagTest, TestCases) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath out_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(temp_dir.GetPath(), &out_file));

  ASSERT_EQ(tagging::BinaryWriteTag(
                test::GetTestFilePath(GetParam().exe_file_name.c_str()),
                GetParam().tag_string, 8206, out_file),
            GetParam().expected_success)
      << GetParam().exe_file_name << ": " << GetParam().tag_string;
  if (GetParam().expected_success) {
    EXPECT_EQ(tagging::BinaryReadTagString(out_file), GetParam().tag_string);
  }
}

struct MsiTagTestMsiReadTagTestCase {
  const std::string msi_file_name;
  const std::optional<tagging::TagArgs> expected_tag_args;
};

class MsiTagTestMsiReadTagTest
    : public ::testing::TestWithParam<MsiTagTestMsiReadTagTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    MsiTagTestMsiReadTagTestCases,
    MsiTagTestMsiReadTagTest,
    ::testing::ValuesIn(std::vector<MsiTagTestMsiReadTagTestCase>{
        // tag:BRAND=QAQA.
        {"GUH-brand-only.msi",
         [] {
           tagging::TagArgs tag_args;
           tag_args.brand_code = "QAQA";
           return tag_args;
         }()},

        // tag:BRAND=QAQA&.
        {"GUH-ampersand-ending.msi",
         [] {
           tagging::TagArgs tag_args;
           tag_args.brand_code = "QAQA";
           return tag_args;
         }()},

        // tag:
        //   appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&
        //   iid={2D8C18E9-8D3A-4EFC-6D61-AE23E3530EA2}&
        //   lang=en&browser=4&usagestats=0&appname=Google%20Chrome&
        //   needsadmin=prefers&brand=CHMB&
        //   installdataindex=defaultbrowser.
        {"GUH-multiple.msi",
         [] {
           tagging::TagArgs tag_args;
           tag_args.bundle_name = "Google Chrome";
           tag_args.installation_id = "{2D8C18E9-8D3A-4EFC-6D61-AE23E3530EA2}";
           tag_args.brand_code = "CHMB";
           tag_args.language = "en";
           tag_args.browser_type = tagging::TagArgs::BrowserType::kChrome;
           tag_args.usage_stats_enable = false;

           tagging::AppArgs app_args("{8A69D345-D564-463C-AFF1-A69D9E530F96}");
           app_args.app_name = "Google Chrome";
           app_args.install_data_index = "defaultbrowser";
           app_args.needs_admin = tagging::NeedsAdmin::kPrefers;
           tag_args.apps = {app_args};

           return tag_args;
         }()},

        // MSI file size greater than `kMaxBufferLength` of 80KB.
        {"GUH-size-greater-than-max.msi",
         [] {
           tagging::TagArgs tag_args;
           tag_args.bundle_name = "Google Chrome Beta";
           tag_args.brand_code = "GGLL";

           tagging::AppArgs app_args("{8237E44A-0054-442C-B6B6-EA0509993955}");
           app_args.app_name = "Google Chrome Beta";
           app_args.needs_admin = tagging::NeedsAdmin::kYes;
           tag_args.apps = {app_args};

           return tag_args;
         }()},

        // special character in the tag value.
        {"GUH-special-value.msi",
         [] {
           tagging::TagArgs tag_args;
           tag_args.brand_code = "QA*A";
           return tag_args;
         }()},

        // tag: =value&BRAND=QAQA.
        {"GUH-empty-key.msi", {}},

        // tag: BRAND=.
        {"GUH-empty-value.msi", {}},

        // tag:(empty string).
        {"GUH-empty-tag.msi", {}},

        // invalid magic signature "Gact2.0Foo".
        {"GUH-invalid-marker.msi", {}},

        // invalid characters in the tag key.
        {"GUH-invalid-key.msi", {}},

        // invalid tag format.
        {"GUH-bad-format.msi", {}},

        // invalid tag format.
        {"GUH-bad-format2.msi", {}},

        // untagged.
        {"GUH-untagged.msi", {}},
    }));

TEST_P(MsiTagTestMsiReadTagTest, TestCases) {
  const auto tag_args =
      tagging::BinaryReadTag(test::GetTestFilePath("tagged_msi")
                                 .AppendASCII(GetParam().msi_file_name));
  EXPECT_EQ(tag_args.has_value(), GetParam().expected_tag_args.has_value());
  if (GetParam().expected_tag_args) {
    test::ExpectTagArgsEqual(*tag_args, *GetParam().expected_tag_args);
  }
}

struct MsiTagTestMsiWriteTagTestCase {
  const std::string msi_file_name;
  const std::string tag_string;
  const bool expected_success;
};

class MsiTagTestMsiWriteTagTest
    : public ::testing::TestWithParam<MsiTagTestMsiWriteTagTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    MsiTagTestMsiWriteTagTestCases,
    MsiTagTestMsiWriteTagTest,
    ::testing::ValuesIn(std::vector<MsiTagTestMsiWriteTagTestCase>{
        // single tag parameter.
        {"GUH-untagged.msi", "brand=QAQA", true},

        // single tag parameter ending in an ampersand.
        {"GUH-untagged.msi", "brand=QAQA&", true},

        // multiple tag parameters.
        {"GUH-untagged.msi",
         "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&iid={2D8C18E9-8D3A-"
         "4EFC-"
         "6D61-AE23E3530EA2}&lang=en&browser=4&usagestats=0&appname=Google%"
         "20Chrome&needsadmin=prefers&brand=CHMB&installdataindex="
         "defaultbrowser",
         true},

        // empty tag string.
        {"GUH-untagged.msi", "", true},

        // already tagged.
        {"GUH-brand-only.msi", "brand=QAQA", true},

        // unknown tag argument `unknowntagarg`.
        {"GUH-untagged.msi",
         "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&iid={2D8C18E9-8D3A-"
         "4EFC-"
         "6D61-AE23E3530EA2}&unknowntagarg=foo",
         false},
    }));

TEST_P(MsiTagTestMsiWriteTagTest, TestCases) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath out_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(temp_dir.GetPath(), &out_file));
  const base::FilePath in_out_file = out_file.AddExtensionASCII(".msi");
  const base::FilePath msi_file_path =
      test::GetTestFilePath("tagged_msi").AppendASCII(GetParam().msi_file_name);
  ASSERT_TRUE(base::CopyFile(msi_file_path, in_out_file));

  for (const auto& [msi_file, out_msi_file] :
       {std::make_pair(msi_file_path, out_file),
        std::make_pair(in_out_file, base::FilePath())}) {
    ASSERT_EQ(tagging::BinaryWriteTag(msi_file, GetParam().tag_string, 8206,
                                      out_msi_file),
              GetParam().expected_success);
    if (GetParam().expected_success && !GetParam().tag_string.empty()) {
      tagging::TagArgs tag_args;
      ASSERT_EQ(tagging::Parse(GetParam().tag_string, {}, tag_args),
                tagging::ErrorCode::kSuccess);
      test::ExpectTagArgsEqual(
          tagging::BinaryReadTag(!out_msi_file.empty() ? out_msi_file
                                                       : msi_file)
              .value(),
          tag_args);
    }
  }
}

}  // namespace updater
