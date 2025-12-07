// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/cloud_print_cdd_conversion.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "printing/printing_features.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace cloud_print {

namespace {

using testing::Eq;
using testing::Pointee;

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

constexpr char kExpectedFitToPageValues[] = R"json({
   "option": [ {
      "type": "AUTO"
   }, {
      "type": "AUTO_FIT"
   }, {
      "type": "FILL"
   }, {
      "type": "FIT"
   }, {
      "type": "NONE"
   }, {
      "is_default": true,
      "type": "FIT"
   }
]})json";

constexpr char kExpectedFitToPageValues2[] = R"json({
   "option": [ {
      "type": "FILL"
   }, {
      "type": "NONE"
   }, {
      "type": "AUTO"
   }, {
      "type": "FIT"
   }, {
      "type": "AUTO_FIT"
   }, {
      "is_default": true,
      "type": "FILL"
   }
]})json";

constexpr char kExpectedFitToPageSingleValue[] = R"json({
   "option": [ {
      "type": "AUTO_FIT"
   }, {
      "is_default": true,
      "type": "AUTO_FIT"
   }
]})json";

constexpr char kExpectedMargins[] = R"json({
  "option": [
    {
      "bottom_microns": 200,
      "left_microns": 200,
      "right_microns": 100,
      "top_microns": 100,
      "is_default": true
    }, {
      "bottom_microns": 0,
      "left_microns": 0,
      "right_microns": 0,
      "top_microns": 0
    }
]})json";

constexpr char kExpectedMarginsWiderPaper[] = R"json({
  "option": [
    {
      "bottom_microns": 200,
      "left_microns": 200,
      "right_microns": 100,
      "top_microns": 100,
      "is_default": true
    }, {
      "bottom_microns": 0,
      "left_microns": 0,
      "right_microns": 0,
      "top_microns": 0
    }, {
      "bottom_microns": 500,
      "left_microns": 1000,
      "right_microns": 700,
      "top_microns": 200
    }
]})json";
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
  EXPECT_THAT(caps_dict->FindString(kKeyVersion),
              testing::Pointee(Eq(kValueVersion)));
  return caps_dict->FindDict(kKeyPrinter);
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
class CloudPrintCddConversionParamTest : public base::test::WithFeatureOverride,
                                         public testing::Test {
 public:
  CloudPrintCddConversionParamTest()
      : base::test::WithFeatureOverride(
            printing::features::kApiPrintingMarginsAndScale) {}

  bool UsePrinterMarginsAndScale() const { return IsParamFeatureEnabled(); }
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(CloudPrintCddConversionParamTest);

TEST_P(CloudPrintCddConversionParamTest, ValidCloudPrintCddConversion) {
#else
TEST(CloudPrintCddConversionTest, ValidCloudPrintCddConversion) {
#endif
  const printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults({});
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);
  ASSERT_TRUE(printer_dict);
  size_t expected_dict_size = 9;
#if BUILDFLAG(IS_CHROMEOS)
  expected_dict_size = UsePrinterMarginsAndScale() ? expected_dict_size + 2
                                                   : expected_dict_size + 1;
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(expected_dict_size, printer_dict->size());
  EXPECT_THAT(
      *printer_dict,
      base::test::IsSupersetOfValue(
          base::Value::Dict()
              .Set("collate",
                   base::test::ParseJson(kExpectedCollateDefaultTrue))
              .Set("color", base::test::ParseJson(kExpectedColor))
              .Set("copies", base::test::ParseJson(kExpectedCopies))
              .Set("dpi", base::test::ParseJson(kExpectedDpi))
              .Set("duplex", base::test::ParseJson(kExpectedDuplex))
              .Set("media_size", base::test::ParseJson(kExpectedMediaSize))
              .Set("media_type", base::test::ParseJson(kExpectedMediaType))
              .Set("page_orientation",
                   base::test::ParseJson(kExpectedPageOrientation))
              .Set("supported_content_type",
                   base::test::ParseJson(kExpectedSupportedContentType))));

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_THAT(printer_dict->Find("pin"),
              Pointee(base::test::IsJson(kExpectedPinSupportedFalse)));
  ASSERT_FALSE(printer_dict->contains("fit_to_page"));
  if (UsePrinterMarginsAndScale()) {
    EXPECT_THAT(printer_dict->Find("margins"),
                Pointee(base::test::IsJson(kExpectedMargins)));
  } else {
    ASSERT_FALSE(printer_dict->contains("margins"));
  }
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
  size_t expected_dict_size = 8;
#if BUILDFLAG(IS_CHROMEOS)
  ++expected_dict_size;
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(expected_dict_size, printer_dict->size());
  ASSERT_FALSE(printer_dict->contains("collate"));
  ASSERT_FALSE(printer_dict->contains("margins"));
}

TEST(CloudPrintCddConversionTest, CollateDefaultIsFalse) {
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults({});
  input.collate_capable = true;
  input.collate_default = false;
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  size_t expected_dict_size = 9;
#if BUILDFLAG(IS_CHROMEOS)
  ++expected_dict_size;
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(expected_dict_size, printer_dict->size());
  EXPECT_THAT(printer_dict->Find("collate"),
              Pointee(base::test::IsJson(kExpectedCollateDefaultFalse)));
}

TEST(CloudPrintCddConversionTest, WiderPaper) {
#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);
#endif  // BUILDFLAG(IS_CHROMEOS)
  // Test that a Paper that has a larger width swaps its width and height when
  // converting to a CDD.  Additionally, create the printable area such that
  // none of the margins are equal.  Create margins as so:  left: 1000,
  // bottom: 500, right: 700, top: 200.
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults({});

  constexpr gfx::Size kPaperSize(127000, 76200);
  constexpr gfx::Rect kPrintableArea(1000, 500, 125300, 75500);
  constexpr int kMaxHeight = 0;
  constexpr bool kHasBorderlessVariant = false;
  // Use const as constexpr fails on Android-x86 builds.
  const std::string kVendorId = "15";
  const std::string kDisplayName = "NA_INDEX_3X5";
#if BUILDFLAG(IS_CHROMEOS)
  input.papers.emplace_back(kDisplayName, kVendorId, kPaperSize, kPrintableArea,
                            kMaxHeight, kHasBorderlessVariant,
                            printing::PaperMargins(200, 700, 500, 1000));
#else
  input.papers.emplace_back(kDisplayName, kVendorId, kPaperSize, kPrintableArea,
                            kMaxHeight, kHasBorderlessVariant);
#endif  // BUILDFLAG(IS_CHROMEOS)
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  size_t expected_dict_size = 9;
#if BUILDFLAG(IS_CHROMEOS)
  expected_dict_size += 2;
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(expected_dict_size, printer_dict->size());
  EXPECT_THAT(printer_dict->Find("media_size"),
              Pointee(base::test::IsJson(kExpectedMediaSizeWithWiderPaper)));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_THAT(printer_dict->Find("margins"),
              Pointee(base::test::IsJson(kExpectedMarginsWiderPaper)));
#endif
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
          printing::SampleWithScaleAndPinAndAdvancedCapabilities());
  base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  size_t expected_dict_size = 11;
  ASSERT_EQ(expected_dict_size, printer_dict->size());
  EXPECT_THAT(
      *printer_dict,
      base::test::IsSupersetOfValue(
          base::Value::Dict()
              .Set("pin", base::test::ParseJson(kExpectedPinSupportedTrue))
              .Set("vendor_capability",
                   base::test::ParseJson(kExpectedAdvancedCapabilities))));
}

TEST(CloudPrintCddConversionTest, MarginsAndFitToPageCapabilities) {
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults(
          printing::SampleWithScaleAndPinAndAdvancedCapabilities());
  base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  size_t expected_dict_size = 11;
  ASSERT_EQ(expected_dict_size, printer_dict->size());
  EXPECT_FALSE(printer_dict->contains("fit_to_page"));
  EXPECT_FALSE(printer_dict->contains("margins"));

  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  output = PrinterSemanticCapsAndDefaultsToCdd(input);
  printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  expected_dict_size += 2;
  ASSERT_EQ(expected_dict_size, printer_dict->size());
  EXPECT_THAT(
      *printer_dict,
      base::test::IsSupersetOfValue(
          base::Value::Dict()
              .Set("fit_to_page",
                   base::test::ParseJson(kExpectedFitToPageValues))
              .Set("margins", base::test::ParseJson(kExpectedMargins))));
}

TEST(CloudPrintCddConversionTest, FitToPageNoCapability) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  printing::PrinterSemanticCapsAndDefaults printer_info;

  base::Value output =
      cloud_print::PrinterSemanticCapsAndDefaultsToCdd(printer_info);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(5u, printer_dict->size());
  EXPECT_FALSE(printer_dict->contains("fit_to_page"));
}

TEST(CloudPrintCddConversionTest, FitToPageSingleValue) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  printing::PrinterSemanticCapsAndDefaults printer_info;
  printer_info.print_scaling_types = {
      printing::mojom::PrintScalingType::kAutoFit};

  base::Value output =
      cloud_print::PrinterSemanticCapsAndDefaultsToCdd(printer_info);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(6u, printer_dict->size());
  EXPECT_TRUE(printer_dict->contains("fit_to_page"));
  EXPECT_THAT(*printer_dict,
              base::test::IsSupersetOfValue(base::Value::Dict().Set(
                  "fit_to_page",
                  base::test::ParseJson(kExpectedFitToPageSingleValue))));
}

TEST(CloudPrintCddConversionTest, FitToPageDefaultValueOnly) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  printing::PrinterSemanticCapsAndDefaults printer_info;
  printer_info.print_scaling_type_default =
      printing::mojom::PrintScalingType::kFit;

  base::Value output =
      cloud_print::PrinterSemanticCapsAndDefaultsToCdd(printer_info);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(5u, printer_dict->size());
  EXPECT_FALSE(printer_dict->contains("fit_to_page"));
}

TEST(CloudPrintCddConversionTest, FitToPageNoDefaultInSupported) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  printing::PrinterSemanticCapsAndDefaults printer_info;
  printer_info.print_scaling_types = {
      printing::mojom::PrintScalingType::kAutoFit};
  printer_info.print_scaling_type_default =
      printing::mojom::PrintScalingType::kFit;

  base::Value output =
      cloud_print::PrinterSemanticCapsAndDefaultsToCdd(printer_info);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(5u, printer_dict->size());
  EXPECT_FALSE(printer_dict->contains("fit_to_page"));
}

TEST(CloudPrintCddConversionTest, FitToPageUnknownDefault) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  printing::PrinterSemanticCapsAndDefaults printer_info;
  printer_info.print_scaling_type_default =
      printing::mojom::PrintScalingType::kUnknownPrintScalingType;
  printer_info.print_scaling_types = {
      printing::mojom::PrintScalingType::kFill,
      printing::mojom::PrintScalingType::kNone,
      printing::mojom::PrintScalingType::kAuto,
      printing::mojom::PrintScalingType::kFit,
      printing::mojom::PrintScalingType::kAutoFit,
      printing::mojom::PrintScalingType::kUnknownPrintScalingType};

  base::Value output =
      cloud_print::PrinterSemanticCapsAndDefaultsToCdd(printer_info);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(6u, printer_dict->size());
  EXPECT_THAT(
      *printer_dict,
      base::test::IsSupersetOfValue(base::Value::Dict().Set(
          "fit_to_page", base::test::ParseJson(kExpectedFitToPageValues2))));
}

TEST(CloudPrintCddConversionTest, FitToPageUnknownsOnly) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  printing::PrinterSemanticCapsAndDefaults printer_info;
  printer_info.print_scaling_type_default =
      printing::mojom::PrintScalingType::kUnknownPrintScalingType;
  printer_info.print_scaling_types = {
      printing::mojom::PrintScalingType::kUnknownPrintScalingType};

  base::Value output =
      cloud_print::PrinterSemanticCapsAndDefaultsToCdd(printer_info);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(5u, printer_dict->size());
  EXPECT_FALSE(printer_dict->contains("fit_to_page"));
}

TEST(CloudPrintCddConversionTest, FitToPageCorrectMapping) {
  base::test::ScopedFeatureList feature_list(
      printing::features::kApiPrintingMarginsAndScale);

  struct ScalingTypeToString {
    printing::mojom::PrintScalingType type;
    std::string str;
  };
  constexpr std::array<ScalingTypeToString, 6> kScalingTypes{
      ScalingTypeToString{printing::mojom::PrintScalingType::kAuto, "AUTO"},
      ScalingTypeToString{printing::mojom::PrintScalingType::kAutoFit,
                          "AUTO_FIT"},
      ScalingTypeToString{printing::mojom::PrintScalingType::kFill, "FILL"},
      ScalingTypeToString{printing::mojom::PrintScalingType::kFit, "FIT"},
      ScalingTypeToString{printing::mojom::PrintScalingType::kNone, "NONE"},
      ScalingTypeToString{
          printing::mojom::PrintScalingType::kUnknownPrintScalingType, ""}};

  for (const auto& value : kScalingTypes) {
    printing::PrinterSemanticCapsAndDefaults printer_info;
    printer_info.print_scaling_types = {value.type};

    base::Value output =
        cloud_print::PrinterSemanticCapsAndDefaultsToCdd(printer_info);
    const base::Value::Dict* printer_dict = GetPrinterDict(output);

    ASSERT_TRUE(printer_dict);
    if (value.type ==
        printing::mojom::PrintScalingType::kUnknownPrintScalingType) {
      ASSERT_EQ(5u, printer_dict->size());
      EXPECT_FALSE(printer_dict->contains("fit_to_page"));
    } else {
      std::string formatted_json = base::StringPrintf(
          R"json({
          "option": [ {
              "type": "%s"
          }, {
              "is_default": true,
              "type": "%s"
          }]
        })json",
          value.str.c_str(), value.str.c_str());
      ASSERT_EQ(6u, printer_dict->size());
      EXPECT_THAT(*printer_dict,
                  base::test::IsSupersetOfValue(base::Value::Dict().Set(
                      "fit_to_page", base::test::ParseJson(formatted_json))));
    }
  }
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
  EXPECT_THAT(printer_dict->Find("vendor_capability"),
              Pointee(base::test::IsJson(kExpectedPageOutputQuality)));
}

TEST(CloudPrintCddConversionTest, PageOutputQualityNullDefaultQuality) {
  printing::PrinterSemanticCapsAndDefaults input =
      printing::GenerateSamplePrinterSemanticCapsAndDefaults(
          printing::SampleWithPageOutputQuality());
  const base::Value output = PrinterSemanticCapsAndDefaultsToCdd(input);
  const base::Value::Dict* printer_dict = GetPrinterDict(output);

  ASSERT_TRUE(printer_dict);
  ASSERT_EQ(10u, printer_dict->size());
  EXPECT_THAT(
      printer_dict->Find("vendor_capability"),
      Pointee(base::test::IsJson(kExpectedPageOutputQualityNullDefault)));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace cloud_print
