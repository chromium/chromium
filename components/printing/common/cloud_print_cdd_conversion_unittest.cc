// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/cloud_print_cdd_conversion.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cloud_print {

namespace {

constexpr char kKeyPrinter[] = "printer";
constexpr char kKeyVersion[] = "version";
constexpr char kValueVersion[] = "1.0";

constexpr char kExpectedCollateDefaultTrue[] = R"json({
})json";

constexpr char kExpectedCollateDefaultFalse[] = R"json({
  "default": false
})json";

constexpr char kExpectedColor[] = R"json({
  "option": [
    {
      "is_default": true,
      "type": "STANDARD_COLOR",
      "vendor_id": "9"
    }, {
      "type": "STANDARD_MONOCHROME",
      "vendor_id": "8"
    }
]})json";

constexpr char kExpectedCopies[] = R"json({
  "default": 1,
  "max": 123
})json";

constexpr char kExpectedDpi[] = R"json({
  "option": [
    {
      "horizontal_dpi": 600,
      "is_default": true,
      "vertical_dpi": 600
    }, {
      "horizontal_dpi": 1200,
      "vertical_dpi": 1200
    }, {
      "horizontal_dpi": 1200,
      "vertical_dpi": 600
    }
]})json";

constexpr char kExpectedDuplex[] = R"json({
  "option": [
    {
      "is_default": true,
      "type": "NO_DUPLEX"
    }, {
      "type": "LONG_EDGE"
    }, {
      "type": "SHORT_EDGE"
    }
]})json";

constexpr char kExpectedMediaSize[] = R"json({
  "option": [
    {
      "custom_display_name": "A4",
      "height_microns": 7016,
      "vendor_id": "12",
      "width_microns": 4961
    }, {
      "custom_display_name": "Letter",
      "height_microns": 6600,
      "is_default": true,
      "vendor_id": "45",
      "width_microns": 5100
    }, {
      "custom_display_name": "A3",
      "height_microns": 9921,
      "vendor_id": "67",
      "width_microns": 7016
    }, {
      "custom_display_name": "Ledger",
      "height_microns": 10200,
      "vendor_id": "89",
      "width_microns": 6600
    }
]})json";

constexpr char kExpectedPageOrientation[] = R"json({
  "option": [
    {
      "is_default": true,
      "type": "PORTRAIT"
    }, {
      "type": "LANDSCAPE"
    }, {
      "type": "AUTO"
    }
]})json";

constexpr char kExpectedSupportedContentType[] = R"json([
  {
    "content_type": "application/pdf"
  }
])json";

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kExpectedPinSupportedTrue[] = R"json({
  "supported": true
})json";

constexpr char kExpectedPinSupportedFalse[] = R"json({
  "supported": false
})json";

constexpr char kExpectedAdvancedCapabilities[] = R"json([
  {
    "display_name": "Advanced Capability #1 (bool)",
    "id": "advanced_cap_bool",
    "type": "TYPED_VALUE",
    "typed_value_cap": {
      "value_type": "BOOLEAN"
    }
  }, {
    "display_name": "Advanced Capability #2 (double)",
    "id": "advanced_cap_double",
    "select_cap": {
      "option": [ {
        "display_name": "Advanced Capability #1",
        "value": "adv_cap_val_1"
      }, {
        "display_name": "Advanced Capability #2",
        "value": "adv_cap_val_2"
      }, {
        "display_name": "Advanced Capability #3",
        "value": "adv_cap_val_3"
      } ]
    },
    "type": "SELECT"
  }
])json";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
constexpr char kExpectedPageOutputQuality[] = R"json([
  {
    "display_name": "Page output quality",
    "id": "page_output_quality",
    "select_cap": {
      "option": [ {
        "display_name": "Normal",
        "value": "ns0000:Normal"
      }, {
        "display_name": "Draft",
        "value": "ns0000:Draft",
        "is_default": true
      }, {
        "display_name": "Custom Settings",
        "value": "ns0000:AdvancedSetting"
      } ]
    },
    "type": "SELECT"
  }
])json";

constexpr char kExpectedPageOutputQualityNullDefault[] = R"json([
  {
    "display_name": "Page output quality",
    "id": "page_output_quality",
    "select_cap": {
      "option": [ {
        "display_name": "Normal",
        "value": "ns0000:Normal"
      }, {
        "display_name": "Draft",
        "value": "ns0000:Draft"
      }, {
        "display_name": "Custom Settings",
        "value": "ns0000:AdvancedSetting"
      } ]
    },
    "type": "SELECT"
  }
])json";
#endif  // BUILDFLAG(IS_WIN)

const printing::PrinterSemanticCapsAndDefaults::Paper kPaperA3{
    /*display_name=*/"A3", /*vendor_id=*/"67",
    /*size_um=*/gfx::Size(7016, 9921)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperA4{
    /*display_name=*/"A4", /*vendor_id=*/"12",
    /*size_um=*/gfx::Size(4961, 7016)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperLetter{
    /*display_name=*/"Letter", /*vendor_id=*/"45",
    /*size_um=*/gfx::Size(5100, 6600)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperLedger{
    /*display_name=*/"Ledger", /*vendor_id=*/"89",
    /*size_um=*/gfx::Size(6600, 10200)};

#if BUILDFLAG(IS_CHROMEOS)
const printing::AdvancedCapability kAdvancedCapability1(
    /*name=*/"advanced_cap_bool",
    /*display_name=*/"Advanced Capability #1 (bool)",
    /*type=*/printing::AdvancedCapability::Type::kBoolean,
    /*default_value=*/"true",
    /*values=*/{});
const printing::AdvancedCapability kAdvancedCapability2(
    /*name=*/"advanced_cap_double",
    /*display_name=*/"Advanced Capability #2 (double)",
    /*type=*/printing::AdvancedCapability::Type::kFloat,
    /*default_value=*/"3.14159",
    /*values=*/
    {
        printing::AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_1",
            /*display_name=*/"Advanced Capability #1"),
        printing::AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_2",
            /*display_name=*/"Advanced Capability #2"),
        printing::AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_3",
            /*display_name=*/"Advanced Capability #3"),
    });
const printing::AdvancedCapabilities kAdvancedCapabilities{
    kAdvancedCapability1, kAdvancedCapability2};
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
const printing::PageOutputQuality
    kPageOutputQuality(/*qualities=*/
                       {
                           printing::PageOutputQualityAttribute(
                               /*display_name=*/"Normal",
                               /*name=*/"ns0000:Normal"),
                           printing::PageOutputQualityAttribute(
                               /*display_name=*/"Draft",
                               /*name=*/"ns0000:Draft"),
                           printing::PageOutputQualityAttribute(
                               /*display_name=*/"Custom Settings",
                               /*name=*/"ns0000:AdvancedSetting"),
                       },
                       /*default_quality=*/"ns0000:Draft");
#endif  // BUILDFLAG(IS_WIN)

constexpr bool kCollateCapable = true;
constexpr bool kCollateDefault = true;

constexpr int32_t kCopiesMax = 123;

const std::vector<printing::mojom::DuplexMode> kDuplexModes{
    printing::mojom::DuplexMode::kSimplex,
    printing::mojom::DuplexMode::kLongEdge,
    printing::mojom::DuplexMode::kShortEdge};
constexpr printing::mojom::DuplexMode kDuplexDefault =
    printing::mojom::DuplexMode::kSimplex;

constexpr bool kColorChangeable = true;
constexpr bool kColorDefault = true;
constexpr printing::mojom::ColorModel kColorModel =
    printing::mojom::ColorModel::kRGB;
constexpr printing::mojom::ColorModel kBwModel =
    printing::mojom::ColorModel::kGrayscale;
const printing::PrinterSemanticCapsAndDefaults::Papers kPapers{kPaperA4,
                                                               kPaperLetter};
const printing::PrinterSemanticCapsAndDefaults::Papers kUserDefinedPapers{
    kPaperA3, kPaperLedger};
const printing::PrinterSemanticCapsAndDefaults::Paper kDefaultPaper =
    kPaperLetter;

constexpr gfx::Size kDpi600(600, 600);
constexpr gfx::Size kDpi1200(1200, 1200);
constexpr gfx::Size kDpi1200x600(1200, 600);
const std::vector<gfx::Size> kDpis{kDpi600, kDpi1200, kDpi1200x600};
constexpr gfx::Size kDefaultDpi = kDpi600;

printing::PrinterSemanticCapsAndDefaults
GenerateSamplePrinterSemanticCapsAndDefaults() {
  printing::PrinterSemanticCapsAndDefaults caps;

  caps.collate_capable = kCollateCapable;
  caps.collate_default = kCollateDefault;
  caps.copies_max = kCopiesMax;
  caps.duplex_modes = kDuplexModes;
  caps.duplex_default = kDuplexDefault;
  caps.color_changeable = kColorChangeable;
  caps.color_default = kColorDefault;
  caps.color_model = kColorModel;
  caps.bw_model = kBwModel;
  caps.papers = kPapers;
  caps.user_defined_papers = kUserDefinedPapers;
  caps.default_paper = kPaperLetter;
  caps.dpis = kDpis;
  caps.default_dpi = kDefaultDpi;

  return caps;
}

const base::Value::Dict* GetPrinterDict(const base::Value& caps_value) {
  const base::Value::Dict* caps_dict = caps_value.GetIfDict();
  if (!caps_dict || !caps_dict->contains(kKeyVersion) ||
      caps_dict->size() != 2u) {
    return nullptr;
  }
  base::ExpectDictStringValue(kValueVersion, *caps_dict, kKeyVersion);
  return caps_dict->FindDict(kKeyPrinter);
}

}  // namespace

TEST(CloudPrintCddConversionTest, ValidCloudPrintCddConversion) {
  const printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(9u, printer_dict->size());
#else
  ASSERT_EQ(8u, printer_dict->size());
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::ExpectDictValue(base::test::ParseJson(kExpectedCollateDefaultTrue),
                        *printer_dict, "collate");
  base::ExpectDictValue(base::test::ParseJson(kExpectedColor), *printer_dict,
                        "color");
  base::ExpectDictValue(base::test::ParseJson(kExpectedCopies), *printer_dict,
                        "copies");
  base::ExpectDictValue(base::test::ParseJson(kExpectedDpi), *printer_dict,
                        "dpi");
  base::ExpectDictValue(base::test::ParseJson(kExpectedDuplex), *printer_dict,
                        "duplex");
  base::ExpectDictValue(base::test::ParseJson(kExpectedMediaSize),
                        *printer_dict, "media_size");
  base::ExpectDictValue(base::test::ParseJson(kExpectedPageOrientation),
                        *printer_dict, "page_orientation");
  base::ExpectDictValue(base::test::ParseJson(kExpectedSupportedContentType),
                        *printer_dict, "supported_content_type");
#if BUILDFLAG(IS_CHROMEOS)
  base::ExpectDictValue(base::test::ParseJson(kExpectedPinSupportedFalse),
                        *printer_dict, "pin");
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST(CloudPrintCddConversionTest, MissingEntry) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  input.collate_capable = false;
  input.collate_default = false;
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(8u, printer_dict->size());
#else
  ASSERT_EQ(7u, printer_dict->size());
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(printer_dict->contains("collate"));
}

TEST(CloudPrintCddConversionTest, CollateDefaultIsFalse) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  input.collate_capable = true;
  input.collate_default = false;
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(9u, printer_dict->size());
#else
  ASSERT_EQ(8u, printer_dict->size());
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::ExpectDictValue(base::test::ParseJson(kExpectedCollateDefaultFalse),
                        *printer_dict, "collate");
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(CloudPrintCddConversionTest, PinAndAdvancedCapabilities) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  input.pin_supported = true;
  input.advanced_capabilities = kAdvancedCapabilities;
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(10u, printer_dict->size());
  base::ExpectDictValue(base::test::ParseJson(kExpectedPinSupportedTrue),
                        *printer_dict, "pin");
  base::ExpectDictValue(base::test::ParseJson(kExpectedAdvancedCapabilities),
                        *printer_dict, "vendor_capability");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
TEST(CloudPrintCddConversionTest, PageOutputQualityWithDefaultQuality) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  input.page_output_quality = kPageOutputQuality;
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(9u, printer_dict->size());
  base::ExpectDictValue(base::test::ParseJson(kExpectedPageOutputQuality),
                        *printer_dict, "vendor_capability");
}

TEST(CloudPrintCddConversionTest, PageOutputQualityNullDefaultQuality) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  input.page_output_quality = kPageOutputQuality;
  input.page_output_quality->default_quality = absl::nullopt;
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(9u, printer_dict->size());
  base::ExpectDictValue(
      base::test::ParseJson(kExpectedPageOutputQualityNullDefault),
      *printer_dict, "vendor_capability");
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace cloud_print