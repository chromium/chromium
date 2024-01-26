// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/cloud_print_cdd_conversion.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

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
      "imageable_area_bottom_microns": 200,
      "imageable_area_left_microns": 100,
      "imageable_area_right_microns": 600,
      "imageable_area_top_microns": 1000,
      "vendor_id": "12",
      "width_microns": 4961,
      "has_borderless_variant": true
    }, {
      "custom_display_name": "Letter",
      "height_microns": 6600,
      "imageable_area_bottom_microns": 0,
      "imageable_area_left_microns": 0,
      "imageable_area_right_microns": 5100,
      "imageable_area_top_microns": 6600,
      "is_default": true,
      "vendor_id": "45",
      "width_microns": 5100
    }, {
      "custom_display_name": "A3",
      "height_microns": 9921,
      "imageable_area_bottom_microns": 0,
      "imageable_area_left_microns": 0,
      "imageable_area_right_microns": 7016,
      "imageable_area_top_microns": 9921,
      "vendor_id": "67",
      "width_microns": 7016
    }, {
      "custom_display_name": "Ledger",
      "height_microns": 10200,
      "imageable_area_bottom_microns": 0,
      "imageable_area_left_microns": 0,
      "imageable_area_right_microns": 6600,
      "imageable_area_top_microns": 10200,
      "vendor_id": "89",
      "width_microns": 6600
    }, {
      "custom_display_name": "Custom",
      "is_continuous_feed": true,
      "max_height_microns": 20000,
      "min_height_microns": 5080,
      "width_microns": 2540
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

constexpr char kExpectedMediaSizeWithWiderPaper[] = R"json({
  "option": [
    {
      "custom_display_name": "A4",
      "height_microns": 7016,
      "imageable_area_bottom_microns": 200,
      "imageable_area_left_microns": 100,
      "imageable_area_right_microns": 600,
      "imageable_area_top_microns": 1000,
      "vendor_id": "12",
      "width_microns": 4961,
      "has_borderless_variant": true
    }, {
      "custom_display_name": "Letter",
      "height_microns": 6600,
      "imageable_area_bottom_microns": 0,
      "imageable_area_left_microns": 0,
      "imageable_area_right_microns": 5100,
      "imageable_area_top_microns": 6600,
      "is_default": true,
      "vendor_id": "45",
      "width_microns": 5100
    }, {
      "custom_display_name": "NA_INDEX_3X5",
      "height_microns": 127000,
      "imageable_area_bottom_microns": 700,
      "imageable_area_left_microns": 500,
      "imageable_area_right_microns": 76000,
      "imageable_area_top_microns": 126000,
      "name": "NA_INDEX_3X5",
      "vendor_id": "15",
      "width_microns": 76200
    }, {
      "custom_display_name": "A3",
      "height_microns": 9921,
      "imageable_area_bottom_microns": 0,
      "imageable_area_left_microns": 0,
      "imageable_area_right_microns": 7016,
      "imageable_area_top_microns": 9921,
      "vendor_id": "67",
      "width_microns": 7016
    }, {
      "custom_display_name": "Ledger",
      "height_microns": 10200,
      "imageable_area_bottom_microns": 0,
      "imageable_area_left_microns": 0,
      "imageable_area_right_microns": 6600,
      "imageable_area_top_microns": 10200,
      "vendor_id": "89",
      "width_microns": 6600
    }, {
      "custom_display_name": "Custom",
      "is_continuous_feed": true,
      "max_height_microns": 20000,
      "min_height_microns": 5080,
      "width_microns": 2540
    }
]})json";

constexpr char kExpectedMediaType[] = R"json({
  "option": [
    {
      "custom_display_name": "Plain Paper",
      "is_default": true,
      "vendor_id": "stationery"
    }, {
      "custom_display_name": "Photo Paper",
      "vendor_id": "photographic"
    }
]})json";

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
        "value": "ns000:Normal"
      }, {
        "display_name": "Draft",
        "value": "ns000:Draft",
        "is_default": true
      }, {
        "display_name": "Advance",
        "value": "ns000:Advance"
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
        "value": "ns000:Normal"
      }, {
        "display_name": "Draft",
        "value": "ns000:Draft"
      }, {
        "display_name": "Advance",
        "value": "ns000:Advance"
      } ]
    },
    "type": "SELECT"
  }
])json";
#endif  // BUILDFLAG(IS_WIN)

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
      printing::GenerateSamplePrinterSemanticCapsAndDefaults({});
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);
  ASSERT_TRUE(printer_dict);
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(10u, printer_dict->size());
#else
  ASSERT_EQ(9u, printer_dict->size());
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
  base::ExpectDictValue(base::test::ParseJson(kExpectedMediaType),
                        *printer_dict, "media_type");
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
      printing::GenerateSamplePrinterSemanticCapsAndDefaults({});
  input.collate_capable = false;
  input.collate_default = false;
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(9u, printer_dict->size());
#else
  ASSERT_EQ(8u, printer_dict->size());
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(printer_dict->contains("collate"));
}

TEST(CloudPrintCddConversionTest, CollateDefaultIsFalse) {
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults({});
  input.collate_capable = true;
  input.collate_default = false;
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(10u, printer_dict->size());
#else
  ASSERT_EQ(9u, printer_dict->size());
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::ExpectDictValue(base::test::ParseJson(kExpectedCollateDefaultFalse),
                        *printer_dict, "collate");
}

TEST(CloudPrintCddConversionTest, WiderPaper) {
  // Test that a Paper that has a larger width swaps its width and height when
  // converting to a CDD.  Additionally, create the printable area such that
  // none of the margins are equal.  Create margins as so:  left: 1000,
  // bottom: 500, right: 700, top: 200.
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults({});
  input.papers.push_back(printing::PrinterSemanticCapsAndDefaults::Paper(
      "NA_INDEX_3X5", "15", gfx::Size(127000, 76200),
      gfx::Rect(1000, 500, 125300, 75500)));
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(10u, printer_dict->size());
#else
  ASSERT_EQ(9u, printer_dict->size());
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::ExpectDictValue(base::test::ParseJson(kExpectedMediaSizeWithWiderPaper),
                        *printer_dict, "media_size");
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(CloudPrintCddConversionTest, MediaTypeOnlyOne) {
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults({});
  input.media_types = {input.media_types[0]};
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  // The media type list should only be included when more than one media type
  // is supported.
  ASSERT_TRUE(printer_dict);
  EXPECT_FALSE(printer_dict->contains("media_type"));
}

TEST(CloudPrintCddConversionTest, PinAndAdvancedCapabilities) {
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults(
          printing::SampleWithPinAndAdvancedCapabilities());
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(11u, printer_dict->size());
  base::ExpectDictValue(base::test::ParseJson(kExpectedPinSupportedTrue),
                        *printer_dict, "pin");
  base::ExpectDictValue(base::test::ParseJson(kExpectedAdvancedCapabilities),
                        *printer_dict, "vendor_capability");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
TEST(CloudPrintCddConversionTest, PageOutputQualityWithDefaultQuality) {
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults(
          printing::SampleWithPageOutputQuality());
  input.page_output_quality->default_quality = printing::kDefaultQuality;
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(10u, printer_dict->size());
  base::ExpectDictValue(base::test::ParseJson(kExpectedPageOutputQuality),
                        *printer_dict, "vendor_capability");
}

TEST(CloudPrintCddConversionTest, PageOutputQualityNullDefaultQuality) {
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults(
          printing::SampleWithPageOutputQuality());
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(10u, printer_dict->size());
  base::ExpectDictValue(
      base::test::ParseJson(kExpectedPageOutputQualityNullDefault),
      *printer_dict, "vendor_capability");
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace cloud_print
