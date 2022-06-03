// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/utility/payment_manifest_parser.h"

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "components/payments/core/error_logger.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

// Payment method manifest parsing:

void ExpectUnableToParsePaymentMethodManifest(const std::string& input) {
  std::vector<GURL> actual_web_app_urls;
  std::vector<url::Origin> actual_supported_origins;

  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(input);

  PaymentManifestParser::ParsePaymentMethodManifestIntoVectors(
      GURL("https://bobpay.com/pmm.json"), std::move(value), ErrorLogger(),
      &actual_web_app_urls, &actual_supported_origins);

  EXPECT_TRUE(actual_web_app_urls.empty()) << actual_web_app_urls.front();
  EXPECT_TRUE(actual_supported_origins.empty())
      << actual_supported_origins.front();
}

void ExpectParsedPaymentMethodManifest(
    const std::string& input,
    const std::vector<GURL>& expected_web_app_urls,
    const std::vector<url::Origin>& expected_supported_origins) {
  std::vector<GURL> actual_web_app_urls;
  std::vector<url::Origin> actual_supported_origins;

  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(input);

  PaymentManifestParser::ParsePaymentMethodManifestIntoVectors(
      GURL("https://bobpay.com/pmm.json"), std::move(value), ErrorLogger(),
      &actual_web_app_urls, &actual_supported_origins);

  EXPECT_EQ(expected_web_app_urls, actual_web_app_urls);
  EXPECT_EQ(expected_supported_origins, actual_supported_origins);
}

TEST(PaymentManifestParserTest, NullPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest(std::string());
}

TEST(PaymentManifestParserTest, NonJsonPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("this is not json");
}

TEST(PaymentManifestParserTest, StringPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("\"this is a string\"");
}

TEST(PaymentManifestParserTest,
     EmptyDictionaryPaymentMethodManifestIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{}");
}

TEST(PaymentManifestParserTest, NullDefaultApplicationIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": null}");
}

TEST(PaymentManifestParserTest, NumberDefaultApplicationIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": 0}");
}

TEST(PaymentManifestParserTest, ListOfNumbersDefaultApplicationIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": [0]}");
}

TEST(PaymentManifestParserTest, EmptyListOfDefaultApplicationsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"default_applications\": []}");
}

TEST(PaymentManifestParserTest, ListOfEmptyDefaultApplicationsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": [\"\"]}");
}

TEST(PaymentManifestParserTest, DefaultApplicationCanBeRelativeURL) {
  ExpectParsedPaymentMethodManifest(
      "{\"default_applications\": [\"manifest.json\"]}",
      {GURL("https://bobpay.com/manifest.json")}, {});
}

TEST(PaymentManifestParserTest, DefaultApplicationsShouldNotHaveNulCharacters) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": [\"https://bobpay.com/app\0json\"]}");
}

TEST(PaymentManifestParserTest, DefaultApplicationsShouldBeUTF8) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\":[\"https://b\x0f\x7f\xf0\xff!\"]}");
}

TEST(PaymentManifestParserTest, DefaultApplicationKeyShouldBeLowercase) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"Default_Applications\": [\"https://bobpay.com/app.json\"]}");
}

TEST(PaymentManifestParserTest,
     DefaultApplicationsCanBeEitherAbsoluteOrRelative) {
  ExpectParsedPaymentMethodManifest(
      "{\"default_applications\": ["
      "\"https://bobpay.com/app1.json\","
      "\"app2.json\"]}",
      {GURL("https://bobpay.com/app1.json"),
       GURL("https://bobpay.com/app2.json")},
      {});
}

TEST(PaymentManifestParserTest, DefaultApplicationsShouldBeHttps) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": ["
      "\"https://bobpay.com/app.json\","
      "\"http://alicepay.com/app.json\"]}");
}

TEST(PaymentManifestParserTest, NullSupportedOriginsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"supported_origins\": null}");
}

TEST(PaymentManifestParserTest, NumberSupportedOriginsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"supported_origins\": 0}");
}

TEST(PaymentManifestParserTest, EmptyListSupportedOriginsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"supported_origins\": []}");
}

TEST(PaymentManifestParserTest, ListOfNumbersSupportedOriginsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"supported_origins\": [0]}");
}

TEST(PaymentManifestParserTest, ListOfEmptySupportedOriginsIsMalformed) {
  ExpectUnableToParsePaymentMethodManifest("{\"supported_origins\": [\"\"]}");
}

TEST(PaymentManifestParserTest, SupportedOriginsShouldNotHaveNulCharacters) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"supported_origins\": [\"https://bob\0pay.com\"]}");
}

TEST(PaymentManifestParserTest, SupportedOriginsShouldBeUTF8) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"supported_origins\":[\"https://b\x0f\x7f\xf0\xff!\"]}");
}

TEST(PaymentManifestParserTest, SupportedOriginsShouldBeHttps) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"supported_origins\": [\"http://bobpay.com\"]}");
}

TEST(PaymentManifestParserTest, SupportedOriginsShouldNotHavePath) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"supported_origins\": [\"https://bobpay.com/webpay\"]}");
}

TEST(PaymentManifestParserTest, SupportedOriginsShouldNotHaveQuery) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"supported_origins\": [\"https://bobpay.com/?action=webpay\"]}");
}

TEST(PaymentManifestParserTest, SupportedOriginsShouldNotHaveRef) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"supported_origins\": [\"https://bobpay.com/#webpay\"]}");
}

TEST(PaymentManifestParserTest, SupportedOriginsShouldBeList) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"supported_origins\": \"https://bobpay.com\"}");
}

TEST(PaymentManifestParserTest, WellFormedPaymentMethodManifestWithApps) {
  ExpectParsedPaymentMethodManifest(
      "{\"default_applications\": ["
      "\"https://bobpay.com/app.json\","
      "\"https://alicepay.com/app.json\"]}",
      {GURL("https://bobpay.com/app.json"),
       GURL("https://alicepay.com/app.json")},
      std::vector<url::Origin>());
}

TEST(PaymentManifestParserTest,
     WellFormedPaymentMethodManifestWithHttpLocalhostApps) {
  ExpectParsedPaymentMethodManifest(
      "{\"default_applications\": ["
      "\"http://127.0.0.1:8080/app.json\","
      "\"http://localhost:8081/app.json\"]}",
      {GURL("http://127.0.0.1:8080/app.json"),
       GURL("http://localhost:8081/app.json")},
      std::vector<url::Origin>());
}

TEST(PaymentManifestParserTest,
     InvalidPaymentMethodManifestWithAppsAndAllSupportedOrigins) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": [\"https://bobpay.com/app.json\", "
      "\"https://alicepay.com/app.json\"], \"supported_origins\": \"*\"}");
}

TEST(PaymentManifestParserTest, OriginWildcardNotSupported) {
  ExpectUnableToParsePaymentMethodManifest("{\"supported_origins\": \"*\"}");
}

TEST(PaymentManifestParserTest,
     InvalidDefaultAppsWillPreventParsingSupportedOrigins) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": [\"http://bobpay.com/app.json\"]}");
}

TEST(PaymentManifestParserTest,
     InvalidSupportedOriginsWillPreventParsingDefaultApps) {
  ExpectUnableToParsePaymentMethodManifest(
      "{\"default_applications\": [\"https://bobpay.com/app.json\"], "
      "\"supported_origins\": \"+\"}");
}

TEST(PaymentManifestParserTest,
     WellFormedPaymentMethodManifestWithAppsAndSomeSupportedOrigins) {
  ExpectParsedPaymentMethodManifest(
      "{\"default_applications\": [\"https://bobpay.com/app.json\", "
      "\"https://alicepay.com/app.json\"], \"supported_origins\": "
      "[\"https://charliepay.com\", \"https://evepay.com\"]}",
      {GURL("https://bobpay.com/app.json"),
       GURL("https://alicepay.com/app.json")},
      {url::Origin::Create(GURL("https://charliepay.com")),
       url::Origin::Create(GURL("https://evepay.com"))});
}

TEST(PaymentManifestParserTest,
     WellFormedPaymentMethodManifestWithHttpLocalhostSupportedOrigins) {
  ExpectParsedPaymentMethodManifest(
      "{\"supported_origins\": [\"http://localhost:8080\", "
      "\"http://127.0.0.1:8081\"]}",
      std::vector<GURL>(),
      {url::Origin::Create(GURL("http://localhost:8080")),
       url::Origin::Create(GURL("http://127.0.0.1:8081"))});
}

TEST(PaymentManifestParserTest,
     WellFormedPaymentMethodManifestWithSomeSupportedOrigins) {
  ExpectParsedPaymentMethodManifest(
      "{\"supported_origins\": [\"https://charliepay.com\", "
      "\"https://evepay.com\"]}",
      std::vector<GURL>(),
      {url::Origin::Create(GURL("https://charliepay.com")),
       url::Origin::Create(GURL("https://evepay.com"))});
}

TEST(PaymentManifestParserTest,
     WellFormedPaymentMethodManifestWithAllSupportedOrigins) {
  ExpectParsedPaymentMethodManifest("{\"supported_origins\": \"*\"}",
                                    std::vector<GURL>(),
                                    std::vector<url::Origin>());
}

// Web app manifest parsing:

void ExpectUnableToParseWebAppManifest(const std::string& input) {
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(input);
  std::vector<WebAppManifestSection> sections;
  PaymentManifestParser::ParseWebAppManifestIntoVector(
      std::move(value), ErrorLogger(), &sections);
  EXPECT_TRUE(sections.empty());
}

void ExpectParsedWebAppManifest(
    const std::string& input,
    const std::string& expected_id,
    int64_t expected_min_version,
    const std::vector<std::vector<uint8_t>>& expected_fingerprints) {
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(input);
  std::vector<WebAppManifestSection> sections;
  EXPECT_TRUE(PaymentManifestParser::ParseWebAppManifestIntoVector(
      std::move(value), ErrorLogger(), &sections));
  ASSERT_EQ(1U, sections.size());
  EXPECT_EQ(expected_id, sections.front().id);
  EXPECT_EQ(expected_min_version, sections.front().min_version);
  EXPECT_EQ(expected_fingerprints, sections.front().fingerprints);
}

TEST(PaymentManifestParserTest, NullContentIsMalformed) {
  ExpectUnableToParseWebAppManifest(std::string());
}

TEST(PaymentManifestParserTest, NonJsonContentIsMalformed) {
  ExpectUnableToParseWebAppManifest("this is not json");
}

TEST(PaymentManifestParserTest, StringContentIsMalformed) {
  ExpectUnableToParseWebAppManifest("\"this is a string\"");
}

TEST(PaymentManifestParserTest, EmptyDictionaryIsMalformed) {
  ExpectUnableToParseWebAppManifest("{}");
}

TEST(PaymentManifestParserTest, NullRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest("{\"related_applications\": null}");
}

TEST(PaymentManifestParserTest, NumberRelatedApplicationsectionIsMalformed) {
  ExpectUnableToParseWebAppManifest("{\"related_applications\": 0}");
}

TEST(PaymentManifestParserTest,
     ListOfNumbersRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest("{\"related_applications\": [0]}");
}

TEST(PaymentManifestParserTest,
     ListOfEmptyDictionariesRelatedApplicationsSectionIsMalformed) {
  ExpectUnableToParseWebAppManifest("{\"related_applications\": [{}]}");
}

TEST(PaymentManifestParserTest, NoPlayPlatformIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoPackageNameIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoVersionIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoFingerprintIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\""
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, EmptyFingerprintsIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": []"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, EmptyFingerprintsDictionaryIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{}]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoFingerprintTypeIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, NoFingerprintValueIsMalformed) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, PlatformShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"pl\0ay\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, PackageNameShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bob\0pay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, VersionShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\01\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintTypeShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_c\0ert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintValueShouldNotHaveNullCharacters) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": "
      "\"00:01:02:0\0:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, KeysShouldBeLowerCase) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"Related_applications\": [{"
      "    \"Platform\": \"play\", "
      "    \"Id\": \"com.bobpay.app\", "
      "    \"Min_version\": \"1\", "
      "    \"Fingerprints\": [{"
      "      \"Type\": \"sha256_cert\", "
      "      \"Value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintsShouldBeSha256) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha1_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintBytesShouldBeColonSeparated) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"000010020030040050060070080090A00A10A20A30A40A50A60A7"
      "0A80A90B00B10B20B30B40B50B60B70B80B90C00C1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintBytesShouldBeUpperCase) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:a0:a1:a2:a3:a4:a5:a6:a7"
      ":a8:a9:b0:b1:b2:b3:b4:b5:b6:b7:b8:b9:c0:c1\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintsShouldContainsThirtyTwoBytes) {
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1:C2\""
      "    }]"
      "  }]"
      "}");
  ExpectUnableToParseWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0\""
      "    }]"
      "  }]"
      "}");
}

TEST(PaymentManifestParserTest, WellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}",
      "com.bobpay.app", 1,
      {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}});
}

TEST(PaymentManifestParserTest, DuplicateSignaturesWellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }, {"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}",
      "com.bobpay.app", 1,
      {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1},
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}});
}

TEST(PaymentManifestParserTest, TwoDifferentSignaturesWellFormed) {
  ExpectParsedWebAppManifest(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }, {"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": \"AA:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}",
      "com.bobpay.app", 1,
      {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1},
       {0xAA, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}});
}

TEST(PaymentManifestParserTest, TwoRelatedApplicationsWellFormed) {
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(
      "{"
      "  \"related_applications\": [{"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app.dev\", "
      "    \"min_version\": \"2\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": "
      "\"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }, {"
      "    \"platform\": \"play\", "
      "    \"id\": \"com.bobpay.app.prod\", "
      "    \"min_version\": \"1\", "
      "    \"fingerprints\": [{"
      "      \"type\": \"sha256_cert\", "
      "      \"value\": "
      "\"AA:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7"
      ":A8:A9:B0:B1:B2:B3:B4:B5:B6:B7:B8:B9:C0:C1\""
      "    }]"
      "  }]"
      "}");
  ASSERT_TRUE(value);

  std::vector<WebAppManifestSection> sections;
  EXPECT_TRUE(PaymentManifestParser::ParseWebAppManifestIntoVector(
      std::move(value), ErrorLogger(), &sections));

  ASSERT_EQ(2U, sections.size());

  EXPECT_EQ("com.bobpay.app.dev", sections.front().id);
  EXPECT_EQ(2, sections.front().min_version);
  EXPECT_EQ(
      std::vector<std::vector<uint8_t>>(
          {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
            0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
            0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}}),
      sections.front().fingerprints);

  EXPECT_EQ("com.bobpay.app.prod", sections.back().id);
  EXPECT_EQ(1, sections.back().min_version);
  EXPECT_EQ(
      std::vector<std::vector<uint8_t>>(
          {{0xAA, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
            0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
            0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}}),
      sections.back().fingerprints);
}

// Web app installation information parsing:

void ExpectUnableToParseInstallInfo(const std::string& input) {
  auto value = base::JSONReader::ReadDeprecated(input);
  auto installation_info = std::make_unique<WebAppInstallationInfo>();
  auto icons =
      std::make_unique<std::vector<PaymentManifestParser::WebAppIcon>>();
  EXPECT_FALSE(PaymentManifestParser::ParseWebAppInstallationInfoIntoStructs(
      std::move(value), ErrorLogger(), installation_info.get(), icons.get()));
}

void ExpectParsedInstallInfo(
    const std::string& input,
    const WebAppInstallationInfo& expected_installation_info,
    const std::vector<PaymentManifestParser::WebAppIcon>& expected_icons) {
  auto value = base::JSONReader::ReadDeprecated(input);
  WebAppInstallationInfo actual_installation_info;
  std::vector<PaymentManifestParser::WebAppIcon> actual_icons;
  EXPECT_TRUE(PaymentManifestParser::ParseWebAppInstallationInfoIntoStructs(
      std::move(value), ErrorLogger(), &actual_installation_info,
      &actual_icons));
  EXPECT_EQ(expected_installation_info.icon == nullptr,
            actual_installation_info.icon == nullptr);
  EXPECT_EQ(expected_installation_info.name, actual_installation_info.name);
  EXPECT_EQ(expected_installation_info.sw_js_url,
            actual_installation_info.sw_js_url);
  EXPECT_EQ(expected_installation_info.sw_scope,
            actual_installation_info.sw_scope);
  EXPECT_EQ(expected_installation_info.sw_use_cache,
            actual_installation_info.sw_use_cache);
  EXPECT_EQ(expected_icons.size(), actual_icons.size());
  for (size_t i = 0; i < expected_icons.size() && i < actual_icons.size();
       ++i) {
    EXPECT_EQ(expected_icons[i].src, actual_icons[i].src);
    EXPECT_EQ(expected_icons[i].sizes, actual_icons[i].sizes);
    EXPECT_EQ(expected_icons[i].type, actual_icons[i].type);
  }
  EXPECT_EQ(expected_installation_info.preferred_app_ids,
            actual_installation_info.preferred_app_ids);
}

TEST(PaymentManifestParserTest, NullInstallInfoIsMalformed) {
  ExpectUnableToParseInstallInfo(std::string());
}

TEST(PaymentManifestParserTest, NonJsonInstallInfoIsMalformed) {
  ExpectUnableToParseInstallInfo("this is not json");
}

TEST(PaymentManifestParserTest, StringInstallInfoIsMalformed) {
  ExpectUnableToParseInstallInfo("\"this is a string\"");
}

TEST(PaymentManifestParserTest, EmptyDictionaryInstallInfoIsMalformed) {
  ExpectUnableToParseInstallInfo("{}");
}

TEST(PaymentManifestParserTest, StringServiceWorkerInInstallInfoIsMalformed) {
  ExpectUnableToParseInstallInfo("{\"serviceworker\": \"sw.js\"}");
}

TEST(PaymentManifestParserTest, IntegerSrcInInstallInfoIsMalformed) {
  ExpectUnableToParseInstallInfo("{\"serviceworker\": {\"src\": 0}}");
}

TEST(PaymentManifestParserTest, NullCharInSrcInInstallInfoIsMalformed) {
  ExpectUnableToParseInstallInfo("{\"serviceworker\": {\"src\": \"\0\"}}");
}

TEST(PaymentManifestParserTest, EmptyServiceWorkerSrcInInstallInfoIsMalformed) {
  ExpectUnableToParseInstallInfo("{\"serviceworker\": {\"src\": \"\"}}");
}

// Test "payment" member format when every other member is well formed.
void TestPaymentMemberFormat(const std::string& payment_format) {
  ExpectUnableToParseInstallInfo(
      "{ "
      "  \"name\": \"Pay with BobPay\","
      "  \"icons\": [{"
      "    \"src\": \"bobpay.png\","
      "    \"sizes\": \"48x48\","
      "    \"type\": \"image/png\""
      "  }],"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\","
      "    \"scope\": \"/some/scope/\","
      "    \"use_cache\": true"
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": [{"
      "      \"platform\": \"play\","
      "      \"id\": \"com.bobpay\""
      "  }]," +
      payment_format + "}");
}

TEST(PaymentManifestParserTest, EmptyPaymentMemberIsMalformed) {
  TestPaymentMemberFormat("\"payment\": {}");
}

TEST(PaymentManifestParserTest, PaymentMemberMustBeADictionary_List) {
  TestPaymentMemberFormat("\"payment\": [\"supported_delegations\"]");
}

TEST(PaymentManifestParserTest, PaymentMemberMustBeADictionary_String) {
  TestPaymentMemberFormat("\"payment\": \"supported_delegations\"");
}

TEST(PaymentManifestParserTest, PaymentMemberMustBeADictionary_Integer) {
  TestPaymentMemberFormat("\"payment\": 1");
}

TEST(PaymentManifestParserTest, InvalidKeyInPaymentMemberIsMalformed) {
  TestPaymentMemberFormat("\"payment\": {\"key\": \"value\"}");
}

TEST(PaymentManifestParserTest, SupportedDelegationsMustBeAList_String) {
  TestPaymentMemberFormat(
      "\"payment\": {\"supported_delegations\": \"string\"}");
}

TEST(PaymentManifestParserTest, SupportedDelegationsMustBeAList_Dictionary) {
  TestPaymentMemberFormat(
      "\"payment\": {\"supported_delegations\": {\"key\": \"value\"}}");
}

TEST(PaymentManifestParserTest, SupportedDelegationsMustBeAList_Integer) {
  TestPaymentMemberFormat("\"payment\": {\"supported_delegations\": 1}");
}

TEST(PaymentManifestParserTest, EmptyListSupportedDelegationsIsMalformed) {
  TestPaymentMemberFormat("\"payment\": {\"supported_delegations\": []}");
}

TEST(PaymentManifestParserTest, InvalidSupportedDelegationsIsMalformed) {
  TestPaymentMemberFormat(
      "\"payment\": {\"supported_delegations\": [\"random string\"]}");
}

TEST(PaymentManifestParserTest,
     NonASCIICharsInSupportedDelegationsIsMalformed) {
  TestPaymentMemberFormat("\"payment\": {\"supported_delegations\": [\"β\"]}");
}

TEST(PaymentManifestParserTest, TooLongSupportedDelegationsListIsMalformed) {
  TestPaymentMemberFormat(
      "\"payment\": {\"supported_delegations\": [" +
      base::JoinString(std::vector<std::string>(101, "\"payerEmail\""), ", ") +
      "]}");
}

TEST(PaymentManifestParserTest, MinimumWellFormedInstallInfo) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  std::vector<PaymentManifestParser::WebAppIcon> expected_icons;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  }"
      "}",
      expected_installation_info, expected_icons);
}

TEST(PaymentManifestParserTest, WellFormedInstallInfo) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.name = "Pay with BobPay";
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "/some/scope/";
  expected_installation_info.sw_use_cache = true;
  expected_installation_info.preferred_app_ids = {"com.bobpay"};
  expected_installation_info.supported_delegations.shipping_address = true;
  expected_installation_info.supported_delegations.payer_email = true;
  expected_installation_info.supported_delegations.payer_name = false;
  expected_installation_info.supported_delegations.payer_phone = false;

  PaymentManifestParser::WebAppIcon expected_icon;
  expected_icon.src = "bobpay.png";
  expected_icon.sizes = "48x48";
  expected_icon.type = "image/png";
  std::vector<PaymentManifestParser::WebAppIcon> expected_icons(1,
                                                                expected_icon);

  ExpectParsedInstallInfo(
      "{"
      "  \"name\": \"Pay with BobPay\","
      "  \"icons\": [{"
      "    \"src\": \"bobpay.png\","
      "    \"sizes\": \"48x48\","
      "    \"type\": \"image/png\""
      "  }],"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\","
      "    \"scope\": \"/some/scope/\","
      "    \"use_cache\": true"
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": [{"
      "      \"platform\": \"play\","
      "      \"id\": \"com.bobpay\""
      "  }],"
      "  \"payment\": {"
      "      \"supported_delegations\": [\"shippingAddress\", \"payerEmail\"]"
      "  }"
      "}",
      expected_installation_info, expected_icons);
}

TEST(PaymentManifestParserTest, IgnoreNonBooleanPreferRelatedApplicationField) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": 20"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, IgnoreFalsePreferRelatedApplicationField) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": false,"
      "  \"related_applications\": [{"
      "      \"platform\": \"play\","
      "      \"id\": \"com.bobpay\""
      "  }]"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, IgnoreEmptyRelatedApplications) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": []"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, IgnoreNonListRelatedApplications) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": {"
      "      \"platform\": \"play\","
      "      \"id\": \"com.bobpay\""
      "  }"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, IgnoreNonDictionaryRelatedApplicationsItems) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": ["
      "      \"com.bobpay\""
      "  ]"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, IgnoreRelatedApplicationsItemsWithoutId) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": [{"
      "      \"platform\": \"play\""
      "  }]"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, IgnoreRelatedApplicationsItemsWithoutPlatform) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": [{"
      "      \"id\": \"com.bobpay\""
      "  }]"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, IgnoreRelatedApplicationsWithoutPlayPlatform) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": [{"
      "      \"platform\": \"web\","
      "      \"id\": \"12345678890\""
      "  }]"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, OneHundredRelatedApplicationsIsWellFormed) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;
  expected_installation_info.preferred_app_ids =
      std::vector<std::string>(100, "com.bobpay");

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": [" +
          base::JoinString(
              std::vector<std::string>(
                  100, "{\"platform\": \"play\", \"id\": \"com.bobpay\"}"),
              ",") +
          "]}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, CapRelatedApplicationsAtOneHundred) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;
  expected_installation_info.preferred_app_ids =
      std::vector<std::string>(100, "com.bobpay");

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": [" +
          base::JoinString(
              std::vector<std::string>(
                  101, "{\"platform\": \"play\", \"id\": \"com.bobpay\"}"),
              ",") +
          "]}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, IgnoreEmptyRelatedApplicationId) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": [{"
      "      \"platform\": \"play\","
      "      \"id\": \"\""
      "  }]"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

TEST(PaymentManifestParserTest, IgnoreNonASCIIRelatedApplicationId) {
  WebAppInstallationInfo expected_installation_info;
  expected_installation_info.sw_js_url = "sw.js";
  expected_installation_info.sw_scope = "";
  expected_installation_info.sw_use_cache = false;

  ExpectParsedInstallInfo(
      "{"
      "  \"serviceworker\": {"
      "    \"src\": \"sw.js\""
      "  },"
      "  \"prefer_related_applications\": true,"
      "  \"related_applications\": [{"
      "      \"platform\": \"play\","
      "      \"id\": \"😊\""
      "  }]"
      "}",
      expected_installation_info,
      std::vector<PaymentManifestParser::WebAppIcon>());
}

}  // namespace
}  // namespace payments
