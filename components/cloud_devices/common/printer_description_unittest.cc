// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/printer_description.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace cloud_devices {

namespace printer {

// Makes sure that same JSON value represented by same strings to simplify
// comparison.
std::string NormalizeJson(const std::string& json) {
  std::string result;
  base::JSONWriter::Write(base::test::ParseJson(json), &result);
  return result;
}

const char kCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "supported_content_type": [ {
          "content_type": "image/pwg-raster"
        }, {
          "content_type": "image/jpeg"
        } ],
        "pwg_raster_config": {
          "document_sheet_back": "MANUAL_TUMBLE",
          "reverse_order_streaming": true
        },
        "color": {
          "option": [ {
            "type": "STANDARD_COLOR"
          }, {
            "type": "STANDARD_MONOCHROME"
          }, {
            "type": "CUSTOM_MONOCHROME",
            "vendor_id": "123",
            "custom_display_name": "monochrome"
          } ]
        },
        "duplex": {
          "option": [ {
            "is_default": true,
            "type": "LONG_EDGE"
           }, {
            "type": "SHORT_EDGE"
           }, {
            "type": "NO_DUPLEX"
           } ]
        },
        "page_orientation": {
          "option": [ {
            "type": "PORTRAIT"
          }, {
            "type": "LANDSCAPE"
          }, {
            "is_default": true,
            "type": "AUTO"
          } ]
        },
        "copies": {
          "default": 1,
          "max": 1
        },
        "margins": {
          "option": [ {
            "is_default": true,
            "type": "BORDERLESS",
            "top_microns": 0,
            "right_microns": 0,
            "bottom_microns": 0,
            "left_microns": 0
          }, {
             "type": "STANDARD",
             "top_microns": 100,
             "right_microns": 200,
             "bottom_microns": 300,
             "left_microns": 400
          }, {
             "type": "CUSTOM",
             "top_microns": 1,
             "right_microns": 2,
             "bottom_microns": 3,
             "left_microns": 4
          } ]
        },
        "dpi": {
          "option": [ {
            "horizontal_dpi": 150,
            "vertical_dpi": 250
          }, {
            "is_default": true,
            "horizontal_dpi": 600,
            "vertical_dpi": 1600
          } ]
        },
        "fit_to_page": {
          "option": [ {
            "is_default": true,
            "type": "NO_FITTING"
          }, {
            "type": "FIT_TO_PAGE"
          }, {
            "type": "GROW_TO_PAGE"
          }, {
            "type": "SHRINK_TO_PAGE"
          }, {
            "type": "FILL_PAGE"
          } ]
        },
        "page_range": {
        },
        "media_size": {
          "option": [ {
            "imageable_area_left_microns": 0,
            "imageable_area_right_microns": 2222,
            "imageable_area_bottom_microns": 0,
            "imageable_area_top_microns": 3333,
            "is_default": true,
            "name": "NA_LETTER",
            "width_microns": 2222,
            "height_microns": 3333
          }, {
            "imageable_area_bottom_microns": 0,
            "imageable_area_left_microns": 0,
            "imageable_area_right_microns": 4444,
            "imageable_area_top_microns": 5555,
            "name": "ISO_A6",
            "width_microns": 4444,
            "height_microns": 5555,
            "has_borderless_variant": true
          }, {
            "imageable_area_bottom_microns": 0,
            "imageable_area_left_microns": 0,
            "imageable_area_right_microns": 6666,
            "imageable_area_top_microns": 7777,
            "name": "JPN_YOU4",
            "width_microns": 6666,
            "height_microns": 7777
          }, {
            "width_microns": 1111,
            "min_height_microns": 2222,
            "max_height_microns": 9999,
            "is_continuous_feed": true,
            "custom_display_name": "Feed",
            "vendor_id": "FEED"
          } ]
        },
        "media_type": {
          "option": [ {
            "custom_display_name": "Plain Paper",
            "vendor_id": "stationery",
            "is_default": true
          }, {
            "custom_display_name": "Photo Paper",
            "vendor_id": "photographic"
          }, {
            "vendor_id": "stationery-lightweight"
          } ]
        },
        "collate": {
          "default": false
        },
        "reverse_order": {
          "default": true
        }
      }
    })";

const char kDefaultCdd[] = R"(
    {
      "version": "1.0"
    })";

const char kBadVersionCdd[] = R"(
    {
      "version": "1.1",
      "printer": {
      }
    })";

const char kMultyDefaultCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "color": {
          "option": [ {
            "is_default": true,
            "type": "STANDARD_COLOR"
          }, {
            "is_default": true,
            "type": "STANDARD_MONOCHROME"
          } ]
        }
      }
    })";

const char kDocumentTypeColorOnlyCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "pwg_raster_config": {
          "document_type_supported": [ "SRGB_8" ],
          "document_sheet_back": "ROTATED"
        }
      }
    })";

const char kDocumentTypeGrayOnlyCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "pwg_raster_config": {
          "document_type_supported": [ "SGRAY_8" ],
          "document_sheet_back": "ROTATED"
        }
      }
    })";

const char kDocumentTypeColorAndGrayCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "pwg_raster_config": {
          "document_type_supported": [ "SRGB_8", "SGRAY_8" ],
          "document_sheet_back": "ROTATED"
        }
      }
    })";

const char kDocumentTypeColorAndUnsupportedCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "pwg_raster_config": {
          "document_type_supported": [ "SRGB_8", "SRGB_16" ],
          "document_sheet_back": "ROTATED"
        }
      }
    })";

const char kDocumentTypeNoneCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "pwg_raster_config": {
          "document_sheet_back": "ROTATED"
        }
      }
    })";

const char kDocumentTypeNotStringCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "pwg_raster_config": {
          "document_type_supported": [ 8, 16 ],
          "document_sheet_back": "ROTATED"
        }
      }
    })";

const char kDocumentTypeNotListCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "pwg_raster_config": {
          "document_type_supported": "ROTATED",
          "document_sheet_back": "ROTATED"
        }
      }
    })";

const char kIntegerRangeVendorCapabilityJson[] = R"(
    {
      "value_type": "INTEGER",
      "min": "0",
      "max": "10"
    })";

const char kFloatDefaultRangeVendorCapabilityJson[] = R"(
    {
      "value_type": "FLOAT",
      "min": "0.0",
      "max": "1.0",
      "default": "0.5"
    })";

const char kInvalidTypeRangeVendorCapabilityJson[] = R"(
    {
      "value_type": "BOOLEAN",
      "min": "0.0",
      "max": "1.0"
    })";

const char kMissingMinValueRangeVendorCapabilityJson[] = R"(
    {
      "value_type": "INT",
      "max": "10"
    })";

const char kInvalidBoundariesRangeVendorCapabilityJson[] = R"(
    {
      "value_type": "INT",
      "min": "10",
      "max": "0"
    })";

const char kInvalidDefaultValueRangeVendorCapabilityJson[] = R"(
    {
      "value_type": "FLOAT",
      "min": "0.0",
      "max": "5.0",
      "default": "10.0"
    })";

const char kSelectVendorCapabilityJson[] = R"(
    {
      "option": [ {
        "value": "value_1",
        "display_name": "name_1"
      }, {
        "value": "value_2",
        "display_name": "name_2",
        "is_default": true
      } ]
    })";

const char kNotListSelectVendorCapabilityJson[] = R"(
    {
      "option": {
        "value": "value",
        "display_name": "name"
      }
    })";

const char kMissingValueSelectVendorCapabilityJson[] = R"(
    {
      "option": [ {
        "display_name": "name"
      } ]
    })";

const char kMissingDisplayNameSelectVendorCapabilityJson[] = R"(
    {
      "option": [ {
        "value": "value"
      } ]
    })";

const char kNoDefaultSelectVendorCapabilityJson[] = R"(
    {
      "option": [ {
        "value": "value_1",
        "display_name": "name_1"
      }, {
        "value": "value_2",
        "display_name": "name_2"
      } ]
    })";

const char kSeveralDefaultsSelectVendorCapabilityJson[] = R"(
    {
      "option": [ {
        "value": "value_1",
        "display_name": "name_1",
        "is_default": true
      }, {
        "value": "value_2",
        "display_name": "name_2",
        "is_default": true
      } ]
    })";

const char kBooleanTypedValueVendorCapabilityJson[] = R"(
    {
      "value_type": "BOOLEAN",
      "default": "true"
    })";

const char kFloatTypedValueVendorCapabilityJson[] = R"(
    {
      "value_type": "FLOAT",
      "default": "1.0"
    })";

const char kIntegerTypedValueVendorCapabilityJson[] = R"(
    {
      "value_type": "INTEGER",
      "default": "10"
    })";

const char kStringTypedValueVendorCapabilityJson[] = R"(
    {
      "value_type": "STRING",
      "default": "value"
    })";

const char kMissingValueTypeTypedValueVendorCapabilityJson[] = R"(
    {
      "default": "value"
    })";

const char kInvalidBooleanTypedValueVendorCapabilityJson[] = R"(
    {
      "value_type": "BOOLEAN",
      "default": "1"
    })";

const char kInvalidFloatTypedValueVendorCapabilityJson[] = R"(
    {
      "value_type": "FLOAT",
      "default": "1.1.1.1"
    })";

const char kInvalidIntegerTypedValueVendorCapabilityJson[] = R"(
    {
      "value_type": "INTEGER",
      "default": "true"
    })";

const char kVendorCapabilityOnlyCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "vendor_capability": [ {
          "id": "id_1",
          "display_name": "name_1",
          "type": "RANGE",
          "range_cap": {
           "value_type": "INTEGER",
           "min": "1",
           "max": "10"
          }
        }, {
          "id": "id_2",
          "display_name": "name_2",
          "type": "SELECT",
          "select_cap": {
            "option": [ {
              "value": "value",
              "display_name": "name",
              "is_default": true
             } ]
          }
        }, {
          "id": "id_3",
          "display_name": "name_3",
          "type": "TYPED_VALUE",
          "typed_value_cap": {
           "value_type": "INTEGER",
           "default": "1"
          }
        } ]
      }
    })";

const char kMissingIdVendorCapabilityCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "vendor_capability": [ {
          "display_name": "name_1",
          "type": "RANGE",
          "range_cap": {
           "value_type": "INTEGER",
           "min": "1",
           "max": "10"
          }
        } ]
      }
    })";

const char kInvalidInnerCapabilityVendorCapabilityCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "vendor_capability": [ {
          "display_name": "name_1",
          "type": "RANGE",
          "range_cap": {
           "value_type": "INTEGER",
           "min": "10",
           "max": "1"
          }
        } ]
      }
    })";

const char kNoInnerCapabilityVendorCapabilityCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "vendor_capability": [ {
          "display_name": "name_1",
          "type": "RANGE"
        } ]
      }
    })";

const char kSeveralInnerCapabilitiesVendorCapabilityCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "vendor_capability": [ {
          "id": "id_1",
          "display_name": "name_1",
          "type": "RANGE",
          "range_cap": {
           "value_type": "INTEGER",
           "min": "1",
           "max": "10"
          },
          "select_cap": {
            "option": [ {
              "value": "value",
              "display_name": "name",
              "is_default": true
             } ]
          }
        } ]
      }
    })";

#if BUILDFLAG(IS_CHROMEOS)
const char kPinOnlyCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "pin": {
          "supported": true
        }
      }
    })";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Invalid because `is_continuous_feed` is true and `min_height_microns` is
// missing.
const char kInvalidCustomMediaNoMinHeightCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "media_size": {
          "option": [ {
            "width_microns": 1111,
            "max_height_microns": 9999,
            "is_continuous_feed": true,
            "custom_display_name": "Feed",
            "vendor_id": "FEED"
          } ]
        }
      }
    })";

// Invalid because `is_continuous_feed` is true and `width_microns` is missing.
const char kInvalidCustomMediaNoWidthCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "media_size": {
          "option": [ {
            "min_height_microns": 1111,
            "max_height_microns": 9999,
            "is_continuous_feed": true,
            "custom_display_name": "Feed",
            "vendor_id": "FEED"
          } ]
        }
      }
    })";

// Invalid because `max_height_microns` is less than `min_height_microns`.
const char kInvalidCustomMediaBadMaxHeightCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "media_size": {
          "option": [ {
            "width_microns": 1111,
            "min_height_microns": 9999,
            "max_height_microns": 2222,
            "is_continuous_feed": true,
            "custom_display_name": "Feed",
            "vendor_id": "FEED"
          } ]
        }
      }
    })";

// Invalid because `min_height_microns` is 0.
const char kInvalidCustomMediaBadMinHeightCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "media_size": {
          "option": [ {
            "width_microns": 1111,
            "min_height_microns": 0,
            "max_height_microns": 2222,
            "is_continuous_feed": true,
            "custom_display_name": "Feed",
            "vendor_id": "FEED"
          } ]
        }
      }
    })";

const char kCjt[] = R"(
    {
      "version": "1.0",
      "print": {
        "pwg_raster_config": {
          "document_sheet_back": "MANUAL_TUMBLE",
          "reverse_order_streaming": true
        },
        "vendor_ticket_item": [
          {
            "id": "finishings",
            "value": "trim"
          }
        ],
        "color": {
          "type": "STANDARD_MONOCHROME"
        },
        "duplex": {
          "type": "NO_DUPLEX"
        },
        "page_orientation": {
          "type": "LANDSCAPE"
        },
        "copies": {
          "copies": 123
        },
        "margins": {
           "type": "CUSTOM",
           "top_microns": 7,
           "right_microns": 6,
           "bottom_microns": 3,
           "left_microns": 1
        },
        "dpi": {
          "horizontal_dpi": 562,
          "vertical_dpi": 125
        },
        "fit_to_page": {
          "type": "SHRINK_TO_PAGE"
        },
        "page_range": {
          "interval": [ {
            "start": 1,
            "end": 99
           }, {
            "start": 150
           } ]
        },
        "media_size": {
          "imageable_area_bottom_microns": 100,
          "imageable_area_left_microns": 300,
          "imageable_area_right_microns": 3961,
          "imageable_area_top_microns": 234,
          "name": "ISO_C7C6",
          "width_microns": 4261,
          "height_microns": 334
        },
        "collate": {
          "collate": false
        },
        "reverse_order": {
          "reverse_order": true
        }
      }
    })";

const char kDefaultCjt[] = R"(
    {
      "version": "1.0"
    })";

const char kBadVersionCjt[] = R"(
    {
      "version": "1.1",
      "print": {
      }
    })";

const struct TestRangeCapabilities {
  const char* const json_name;
  RangeVendorCapability range_capability;
} kTestRangeCapabilities[] = {
    {kIntegerRangeVendorCapabilityJson,
     RangeVendorCapability(RangeVendorCapability::ValueType::INTEGER,
                           "0",
                           "10")},
    {kFloatDefaultRangeVendorCapabilityJson,
     RangeVendorCapability(RangeVendorCapability::ValueType::FLOAT,
                           "0.0",
                           "1.0",
                           "0.5")}};

const struct TestTypedValueCapabilities {
  const char* const json_name;
  TypedValueVendorCapability typed_value_capability;
} kTestTypedValueCapabilities[] = {
    {kBooleanTypedValueVendorCapabilityJson,
     TypedValueVendorCapability(TypedValueVendorCapability::ValueType::BOOLEAN,
                                "true")},
    {kFloatTypedValueVendorCapabilityJson,
     TypedValueVendorCapability(TypedValueVendorCapability::ValueType::FLOAT,
                                "1.0")},
    {kIntegerTypedValueVendorCapabilityJson,
     TypedValueVendorCapability(TypedValueVendorCapability::ValueType::INTEGER,
                                "10")},
    {kStringTypedValueVendorCapabilityJson,
     TypedValueVendorCapability(TypedValueVendorCapability::ValueType::STRING,
                                "value")},
};

TEST(PrinterDescriptionTest, CddInit) {
  CloudDeviceDescription description;
  EXPECT_EQ(NormalizeJson(kDefaultCdd),
            NormalizeJson(description.ToStringForTesting()));

  ContentTypesCapability content_types;
  PwgRasterConfigCapability pwg_raster;
  ColorCapability color;
  DuplexCapability duplex;
  OrientationCapability orientation;
  MarginsCapability margins;
  DpiCapability dpi;
  FitToPageCapability fit_to_page;
  MediaCapability media;
  MediaTypeCapability media_type;
  CopiesCapability copies;
  PageRangeCapability page_range;
  CollateCapability collate;
  ReverseCapability reverse;

  EXPECT_FALSE(content_types.LoadFrom(description));
  EXPECT_FALSE(pwg_raster.LoadFrom(description));
  EXPECT_FALSE(color.LoadFrom(description));
  EXPECT_FALSE(duplex.LoadFrom(description));
  EXPECT_FALSE(orientation.LoadFrom(description));
  EXPECT_FALSE(copies.LoadFrom(description));
  EXPECT_FALSE(margins.LoadFrom(description));
  EXPECT_FALSE(dpi.LoadFrom(description));
  EXPECT_FALSE(fit_to_page.LoadFrom(description));
  EXPECT_FALSE(page_range.LoadFrom(description));
  EXPECT_FALSE(media.LoadFrom(description));
  EXPECT_FALSE(media_type.LoadFrom(description));
  EXPECT_FALSE(collate.LoadFrom(description));
  EXPECT_FALSE(reverse.LoadFrom(description));
  EXPECT_FALSE(media.LoadFrom(description));
}

TEST(PrinterDescriptionTest, CddInvalid) {
  CloudDeviceDescription description;
  ColorCapability color;

  EXPECT_FALSE(description.InitFromString(kBadVersionCdd));

  EXPECT_TRUE(description.InitFromString(kMultyDefaultCdd));
  EXPECT_FALSE(color.LoadFrom(description));
}

TEST(PrinterDescriptionTest, CddSetAll) {
  CloudDeviceDescription description;

  ContentTypesCapability content_types;
  PwgRasterConfigCapability pwg_raster_config;
  ColorCapability color;
  DuplexCapability duplex;
  OrientationCapability orientation;
  MarginsCapability margins;
  DpiCapability dpi;
  FitToPageCapability fit_to_page;
  MediaCapability media;
  MediaTypeCapability media_type;
  CopiesCapability copies;
  PageRangeCapability page_range;
  CollateCapability collate;
  ReverseCapability reverse;

  content_types.AddOption("image/pwg-raster");
  content_types.AddOption("image/jpeg");

  PwgRasterConfig custom_raster;
  custom_raster.document_sheet_back = DocumentSheetBack::MANUAL_TUMBLE;
  custom_raster.reverse_order_streaming = true;
  custom_raster.rotate_all_pages = false;
  pwg_raster_config.set_value(custom_raster);

  color.AddOption(Color(ColorType::STANDARD_COLOR));
  color.AddOption(Color(ColorType::STANDARD_MONOCHROME));
  Color custom(ColorType::CUSTOM_MONOCHROME);
  custom.vendor_id = "123";
  custom.custom_display_name = "monochrome";
  color.AddOption(custom);

  duplex.AddDefaultOption(DuplexType::LONG_EDGE, true);
  duplex.AddOption(DuplexType::SHORT_EDGE);
  duplex.AddOption(DuplexType::NO_DUPLEX);

  orientation.AddOption(OrientationType::PORTRAIT);
  orientation.AddOption(OrientationType::LANDSCAPE);
  orientation.AddDefaultOption(OrientationType::AUTO_ORIENTATION, true);

  margins.AddDefaultOption(Margins(MarginsType::NO_MARGINS, 0, 0, 0, 0), true);
  margins.AddOption(Margins(MarginsType::STANDARD_MARGINS, 100, 200, 300, 400));
  margins.AddOption(Margins(MarginsType::CUSTOM_MARGINS, 1, 2, 3, 4));

  dpi.AddOption(Dpi(150, 250));
  dpi.AddDefaultOption(Dpi(600, 1600), true);

  fit_to_page.AddDefaultOption(FitToPageType::NO_FITTING, true);
  fit_to_page.AddOption(FitToPageType::FIT_TO_PAGE);
  fit_to_page.AddOption(FitToPageType::GROW_TO_PAGE);
  fit_to_page.AddOption(FitToPageType::SHRINK_TO_PAGE);
  fit_to_page.AddOption(FitToPageType::FILL_PAGE);

  media.AddDefaultOption(MediaBuilder()
                             .WithStandardName(MediaSize::NA_LETTER)
                             .WithSizeAndDefaultPrintableArea({2222, 3333})
                             .Build(),
                         true);
  media.AddOption(MediaBuilder()
                      .WithStandardName(MediaSize::ISO_A6)
                      .WithSizeAndDefaultPrintableArea({4444, 5555})
                      .WithBorderlessVariant(true)
                      .Build());
  media.AddOption(MediaBuilder()
                      .WithStandardName(MediaSize::JPN_YOU4)
                      .WithSizeAndDefaultPrintableArea({6666, 7777})
                      .Build());
  media.AddOption(MediaBuilder()
                      .WithCustomName("Feed", "FEED")
                      .WithSizeAndDefaultPrintableArea({1111, 2222})
                      .WithMaxHeight(9999)
                      .WithBorderlessVariant(false)
                      .Build());

  media_type.AddDefaultOption(MediaType("stationery", "Plain Paper"), true);
  media_type.AddOption(MediaType("photographic", "Photo Paper"));
  media_type.AddOption(MediaType("stationery-lightweight", ""));

  collate.set_default_value(false);
  reverse.set_default_value(true);

  content_types.SaveTo(&description);
  color.SaveTo(&description);
  duplex.SaveTo(&description);
  orientation.SaveTo(&description);
  copies.SaveTo(&description);
  margins.SaveTo(&description);
  dpi.SaveTo(&description);
  fit_to_page.SaveTo(&description);
  page_range.SaveTo(&description);
  media.SaveTo(&description);
  media_type.SaveTo(&description);
  collate.SaveTo(&description);
  reverse.SaveTo(&description);
  pwg_raster_config.SaveTo(&description);

  EXPECT_EQ(NormalizeJson(kCdd),
            NormalizeJson(description.ToStringForTesting()));
}

TEST(PrinterDescriptionTest, CddGetDocumentTypeSupported) {
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(kDocumentTypeColorOnlyCdd));

    PwgRasterConfigCapability pwg_raster;
    EXPECT_TRUE(pwg_raster.LoadFrom(description));
    ASSERT_EQ(1U, pwg_raster.value().document_types_supported.size());
    EXPECT_EQ(PwgDocumentTypeSupported::SRGB_8,
              pwg_raster.value().document_types_supported[0]);
    EXPECT_EQ(DocumentSheetBack::ROTATED,
              pwg_raster.value().document_sheet_back);
    EXPECT_FALSE(pwg_raster.value().reverse_order_streaming);
  }
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(kDocumentTypeGrayOnlyCdd));

    PwgRasterConfigCapability pwg_raster;
    EXPECT_TRUE(pwg_raster.LoadFrom(description));
    ASSERT_EQ(1U, pwg_raster.value().document_types_supported.size());
    EXPECT_EQ(PwgDocumentTypeSupported::SGRAY_8,
              pwg_raster.value().document_types_supported[0]);
    EXPECT_EQ(DocumentSheetBack::ROTATED,
              pwg_raster.value().document_sheet_back);
    EXPECT_FALSE(pwg_raster.value().reverse_order_streaming);
  }
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(kDocumentTypeColorAndGrayCdd));

    PwgRasterConfigCapability pwg_raster;
    EXPECT_TRUE(pwg_raster.LoadFrom(description));
    ASSERT_EQ(2U, pwg_raster.value().document_types_supported.size());
    EXPECT_EQ(PwgDocumentTypeSupported::SRGB_8,
              pwg_raster.value().document_types_supported[0]);
    EXPECT_EQ(PwgDocumentTypeSupported::SGRAY_8,
              pwg_raster.value().document_types_supported[1]);
    EXPECT_EQ(DocumentSheetBack::ROTATED,
              pwg_raster.value().document_sheet_back);
    EXPECT_FALSE(pwg_raster.value().reverse_order_streaming);
  }
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(
        description.InitFromString(kDocumentTypeColorAndUnsupportedCdd));

    PwgRasterConfigCapability pwg_raster;
    EXPECT_TRUE(pwg_raster.LoadFrom(description));
    ASSERT_EQ(1U, pwg_raster.value().document_types_supported.size());
    EXPECT_EQ(PwgDocumentTypeSupported::SRGB_8,
              pwg_raster.value().document_types_supported[0]);
    EXPECT_EQ(DocumentSheetBack::ROTATED,
              pwg_raster.value().document_sheet_back);
    EXPECT_FALSE(pwg_raster.value().reverse_order_streaming);
  }
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(kDocumentTypeNoneCdd));

    PwgRasterConfigCapability pwg_raster;
    EXPECT_TRUE(pwg_raster.LoadFrom(description));
    EXPECT_EQ(0U, pwg_raster.value().document_types_supported.size());
    EXPECT_EQ(DocumentSheetBack::ROTATED,
              pwg_raster.value().document_sheet_back);
    EXPECT_FALSE(pwg_raster.value().reverse_order_streaming);
  }
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(kDocumentTypeNotStringCdd));

    PwgRasterConfigCapability pwg_raster;
    EXPECT_FALSE(pwg_raster.LoadFrom(description));
  }
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(kDocumentTypeNotListCdd));

    PwgRasterConfigCapability pwg_raster;
    EXPECT_FALSE(pwg_raster.LoadFrom(description));
  }
}

TEST(PrinterDescriptionTest, CddSetDocumentTypeSupported) {
  {
    CloudDeviceDescription description;

    PwgRasterConfig custom_raster;
    custom_raster.document_types_supported.push_back(
        PwgDocumentTypeSupported::SRGB_8);
    custom_raster.document_sheet_back = DocumentSheetBack::ROTATED;

    PwgRasterConfigCapability pwg_raster;
    pwg_raster.set_value(custom_raster);
    pwg_raster.SaveTo(&description);

    EXPECT_EQ(NormalizeJson(kDocumentTypeColorOnlyCdd),
              NormalizeJson(description.ToStringForTesting()));
  }
  {
    CloudDeviceDescription description;

    PwgRasterConfig custom_raster;
    custom_raster.document_types_supported.push_back(
        PwgDocumentTypeSupported::SGRAY_8);
    custom_raster.document_sheet_back = DocumentSheetBack::ROTATED;

    PwgRasterConfigCapability pwg_raster;
    pwg_raster.set_value(custom_raster);
    pwg_raster.SaveTo(&description);

    EXPECT_EQ(NormalizeJson(kDocumentTypeGrayOnlyCdd),
              NormalizeJson(description.ToStringForTesting()));
  }
  {
    CloudDeviceDescription description;

    PwgRasterConfig custom_raster;
    custom_raster.document_types_supported.push_back(
        PwgDocumentTypeSupported::SRGB_8);
    custom_raster.document_types_supported.push_back(
        PwgDocumentTypeSupported::SGRAY_8);
    custom_raster.document_sheet_back = DocumentSheetBack::ROTATED;

    PwgRasterConfigCapability pwg_raster;
    pwg_raster.set_value(custom_raster);
    pwg_raster.SaveTo(&description);

    EXPECT_EQ(NormalizeJson(kDocumentTypeColorAndGrayCdd),
              NormalizeJson(description.ToStringForTesting()));
  }
  {
    CloudDeviceDescription description;

    PwgRasterConfig custom_raster;
    custom_raster.document_sheet_back = DocumentSheetBack::ROTATED;

    PwgRasterConfigCapability pwg_raster;
    pwg_raster.set_value(custom_raster);
    pwg_raster.SaveTo(&description);

    EXPECT_EQ(NormalizeJson(kDocumentTypeNoneCdd),
              NormalizeJson(description.ToStringForTesting()));
  }
}

TEST(PrinterDescriptionTest, CddGetRangeVendorCapability) {
  for (const auto& capacity : kTestRangeCapabilities) {
    base::Value description = base::test::ParseJson(capacity.json_name);
    ASSERT_TRUE(description.is_dict());
    RangeVendorCapability range_capability;
    EXPECT_TRUE(range_capability.LoadFrom(description.GetDict()));
    EXPECT_EQ(capacity.range_capability, range_capability);
  }

  const char* const kInvalidJsonNames[] = {
      kMissingMinValueRangeVendorCapabilityJson,
      kInvalidTypeRangeVendorCapabilityJson,
      kInvalidBoundariesRangeVendorCapabilityJson,
      kInvalidDefaultValueRangeVendorCapabilityJson};
  for (const char* invalid_json_name : kInvalidJsonNames) {
    base::Value description = base::test::ParseJson(invalid_json_name);
    ASSERT_TRUE(description.is_dict());
    RangeVendorCapability range_capability;
    EXPECT_FALSE(range_capability.LoadFrom(description.GetDict()));
  }
}

TEST(PrinterDescriptionTest, CddSetRangeVendorCapability) {
  for (const auto& capacity : kTestRangeCapabilities) {
    base::Value::Dict range_capability_value;
    capacity.range_capability.SaveTo(&range_capability_value);
    std::string range_capability_str;
    EXPECT_TRUE(base::JSONWriter::WriteWithOptions(
        range_capability_value, base::JSONWriter::OPTIONS_PRETTY_PRINT,
        &range_capability_str));
    EXPECT_EQ(NormalizeJson(capacity.json_name),
              NormalizeJson(range_capability_str));
  }
}

TEST(PrinterDescriptionTest, CddGetSelectVendorCapability) {
  {
    base::Value description =
        base::test::ParseJson(kSelectVendorCapabilityJson);
    ASSERT_TRUE(description.is_dict());
    SelectVendorCapability select_capability;
    EXPECT_TRUE(select_capability.LoadFrom(description.GetDict()));
    EXPECT_EQ(2u, select_capability.size());
    EXPECT_TRUE(select_capability.Contains(
        SelectVendorCapabilityOption("value_1", "name_1")));
    EXPECT_TRUE(select_capability.Contains(
        SelectVendorCapabilityOption("value_2", "name_2")));
    EXPECT_EQ(SelectVendorCapabilityOption("value_2", "name_2"),
              select_capability.GetDefault());
  }
  {
    base::Value description =
        base::test::ParseJson(kNoDefaultSelectVendorCapabilityJson);
    ASSERT_TRUE(description.is_dict());
    SelectVendorCapability select_capability;
    EXPECT_TRUE(select_capability.LoadFrom(description.GetDict()));
    EXPECT_EQ(2u, select_capability.size());
    EXPECT_TRUE(select_capability.Contains(
        SelectVendorCapabilityOption("value_1", "name_1")));
    EXPECT_TRUE(select_capability.Contains(
        SelectVendorCapabilityOption("value_2", "name_2")));
    EXPECT_EQ(SelectVendorCapabilityOption("value_1", "name_1"),
              select_capability.GetDefault());
  }
  const char* const kInvalidJsonNames[] = {
      kNotListSelectVendorCapabilityJson,
      kMissingValueSelectVendorCapabilityJson,
      kMissingDisplayNameSelectVendorCapabilityJson,
      kSeveralDefaultsSelectVendorCapabilityJson};
  for (const char* invalid_json_name : kInvalidJsonNames) {
    base::Value description = base::test::ParseJson(invalid_json_name);
    ASSERT_TRUE(description.is_dict());
    SelectVendorCapability select_capability;
    EXPECT_FALSE(select_capability.LoadFrom(description.GetDict()));
  }
}

TEST(PrinterDescriptionTest, CddSetSelectVendorCapability) {
  {
    SelectVendorCapability select_capability;
    select_capability.AddOption(
        SelectVendorCapabilityOption("value_1", "name_1"));
    select_capability.AddDefaultOption(
        SelectVendorCapabilityOption("value_2", "name_2"), true);
    base::Value::Dict select_capability_value;
    select_capability.SaveTo(&select_capability_value);
    std::string select_capability_str;
    EXPECT_TRUE(base::JSONWriter::WriteWithOptions(
        select_capability_value, base::JSONWriter::OPTIONS_PRETTY_PRINT,
        &select_capability_str));
    EXPECT_EQ(NormalizeJson(kSelectVendorCapabilityJson),
              NormalizeJson(select_capability_str));
  }
  {
    SelectVendorCapability select_capability;
    select_capability.AddOption(
        SelectVendorCapabilityOption("value_1", "name_1"));
    select_capability.AddOption(
        SelectVendorCapabilityOption("value_2", "name_2"));
    base::Value::Dict select_capability_value;
    select_capability.SaveTo(&select_capability_value);
    std::string select_capability_str;
    EXPECT_TRUE(base::JSONWriter::WriteWithOptions(
        select_capability_value, base::JSONWriter::OPTIONS_PRETTY_PRINT,
        &select_capability_str));
    EXPECT_EQ(NormalizeJson(kNoDefaultSelectVendorCapabilityJson),
              NormalizeJson(select_capability_str));
  }
}

TEST(PrinterDescriptionTest, CddGetTypedValueVendorCapability) {
  for (const auto& capacity : kTestTypedValueCapabilities) {
    base::Value description = base::test::ParseJson(capacity.json_name);
    ASSERT_TRUE(description.is_dict());
    TypedValueVendorCapability typed_value_capability;
    EXPECT_TRUE(typed_value_capability.LoadFrom(description.GetDict()));
    EXPECT_EQ(capacity.typed_value_capability, typed_value_capability);
  }

  const char* const kInvalidJsonNames[] = {
      kMissingValueTypeTypedValueVendorCapabilityJson,
      kInvalidBooleanTypedValueVendorCapabilityJson,
      kInvalidFloatTypedValueVendorCapabilityJson,
      kInvalidIntegerTypedValueVendorCapabilityJson};
  for (const char* invalid_json_name : kInvalidJsonNames) {
    base::Value description = base::test::ParseJson(invalid_json_name);
    ASSERT_TRUE(description.is_dict());
    TypedValueVendorCapability typed_value_capability;
    EXPECT_FALSE(typed_value_capability.LoadFrom(description.GetDict()));
  }
}

TEST(PrinterDescriptionTest, CddSetTypedValueVendorCapability) {
  for (const auto& capacity : kTestTypedValueCapabilities) {
    base::Value::Dict typed_value_capability_value;
    capacity.typed_value_capability.SaveTo(&typed_value_capability_value);
    std::string typed_value_capability_str;
    EXPECT_TRUE(base::JSONWriter::WriteWithOptions(
        typed_value_capability_value, base::JSONWriter::OPTIONS_PRETTY_PRINT,
        &typed_value_capability_str));
    EXPECT_EQ(NormalizeJson(capacity.json_name),
              NormalizeJson(typed_value_capability_str));
  }
}

TEST(PrinterDescriptionTest, CddGetVendorCapability) {
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(kVendorCapabilityOnlyCdd));

    VendorCapabilities vendor_capabilities;
    EXPECT_TRUE(vendor_capabilities.LoadFrom(description));
    EXPECT_EQ(3u, vendor_capabilities.size());
    EXPECT_TRUE(vendor_capabilities.Contains(VendorCapability(
        "id_1", "name_1",
        RangeVendorCapability(RangeVendorCapability::ValueType::INTEGER, "1",
                              "10"))));
    SelectVendorCapability select_capability;
    select_capability.AddDefaultOption(
        SelectVendorCapabilityOption("value", "name"), true);
    EXPECT_TRUE(vendor_capabilities.Contains(
        VendorCapability("id_2", "name_2", std::move(select_capability))));
    EXPECT_TRUE(vendor_capabilities.Contains(VendorCapability(
        "id_3", "name_3",
        TypedValueVendorCapability(
            TypedValueVendorCapability::ValueType::INTEGER, "1"))));
  }

  const char* const kInvalidJsonNames[] = {
      kMissingIdVendorCapabilityCdd, kInvalidInnerCapabilityVendorCapabilityCdd,
      kNoInnerCapabilityVendorCapabilityCdd,
      kSeveralInnerCapabilitiesVendorCapabilityCdd};
  for (const char* invalid_json_name : kInvalidJsonNames) {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(invalid_json_name));
    VendorCapabilities vendor_capabilities;
    EXPECT_FALSE(vendor_capabilities.LoadFrom(description));
  }
}

TEST(PrinterDescriptionTest, CddSetVendorCapability) {
  CloudDeviceDescription description;

  VendorCapabilities vendor_capabilities;
  vendor_capabilities.AddOption(VendorCapability(
      "id_1", "name_1",
      RangeVendorCapability(RangeVendorCapability::ValueType::INTEGER, "1",
                            "10")));
  SelectVendorCapability select_capability;
  select_capability.AddDefaultOption(
      SelectVendorCapabilityOption("value", "name"), true);
  vendor_capabilities.AddOption(
      VendorCapability("id_2", "name_2", std::move(select_capability)));
  vendor_capabilities.AddOption(VendorCapability(
      "id_3", "name_3",
      TypedValueVendorCapability(TypedValueVendorCapability::ValueType::INTEGER,
                                 "1")));

  vendor_capabilities.SaveTo(&description);
  EXPECT_EQ(NormalizeJson(kVendorCapabilityOnlyCdd),
            NormalizeJson(description.ToStringForTesting()));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(PrinterDescriptionTest, CddGetPin) {
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(kPinOnlyCdd));

    PinCapability pin_capability;
    EXPECT_TRUE(pin_capability.LoadFrom(description));
    EXPECT_TRUE(pin_capability.value());
  }
  {
    CloudDeviceDescription description;
    ASSERT_TRUE(description.InitFromString(kDefaultCdd));
    PinCapability pin_capability;
    EXPECT_FALSE(pin_capability.LoadFrom(description));
  }
}

TEST(PrinterDescriptionTest, CddSetPin) {
  CloudDeviceDescription description;

  PinCapability pin_capability;
  pin_capability.set_value(true);
  pin_capability.SaveTo(&description);
  EXPECT_EQ(NormalizeJson(kPinOnlyCdd),
            NormalizeJson(description.ToStringForTesting()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST(PrinterDescriptionTest, CddInvalidCustomMediaNoMinHeight) {
  CloudDeviceDescription description;
  ASSERT_TRUE(description.InitFromString(kInvalidCustomMediaNoMinHeightCdd));
  MediaCapability media_capability;
  EXPECT_FALSE(media_capability.LoadFrom(description));
}

TEST(PrinterDescriptionTest, CddInvalidCustomMediaNoWidth) {
  CloudDeviceDescription description;
  ASSERT_TRUE(description.InitFromString(kInvalidCustomMediaNoWidthCdd));
  MediaCapability media_capability;
  EXPECT_FALSE(media_capability.LoadFrom(description));
}

TEST(PrinterDescriptionTest, CddInvalidCustomMediaBadMaxHeight) {
  CloudDeviceDescription description;
  ASSERT_TRUE(description.InitFromString(kInvalidCustomMediaBadMaxHeightCdd));
  MediaCapability media_capability;
  EXPECT_FALSE(media_capability.LoadFrom(description));
}

TEST(PrinterDescriptionTest, CddInvalidCustomMediaBadMinHeight) {
  CloudDeviceDescription description;
  ASSERT_TRUE(description.InitFromString(kInvalidCustomMediaBadMinHeightCdd));
  MediaCapability media_capability;
  EXPECT_FALSE(media_capability.LoadFrom(description));
}

TEST(PrinterDescriptionTest, CddGetAll) {
  CloudDeviceDescription description;
  ASSERT_TRUE(description.InitFromString(kCdd));

  ContentTypesCapability content_types;
  PwgRasterConfigCapability pwg_raster_config;
  ColorCapability color;
  DuplexCapability duplex;
  OrientationCapability orientation;
  MarginsCapability margins;
  DpiCapability dpi;
  FitToPageCapability fit_to_page;
  MediaCapability media;
  MediaTypeCapability media_type;
  CopiesCapability copies;
  PageRangeCapability page_range;
  CollateCapability collate;
  ReverseCapability reverse;

  EXPECT_TRUE(content_types.LoadFrom(description));
  EXPECT_TRUE(color.LoadFrom(description));
  EXPECT_TRUE(duplex.LoadFrom(description));
  EXPECT_TRUE(orientation.LoadFrom(description));
  EXPECT_TRUE(copies.LoadFrom(description));
  EXPECT_TRUE(margins.LoadFrom(description));
  EXPECT_TRUE(dpi.LoadFrom(description));
  EXPECT_TRUE(fit_to_page.LoadFrom(description));
  EXPECT_TRUE(page_range.LoadFrom(description));
  EXPECT_TRUE(media.LoadFrom(description));
  EXPECT_TRUE(media_type.LoadFrom(description));
  EXPECT_TRUE(collate.LoadFrom(description));
  EXPECT_TRUE(reverse.LoadFrom(description));
  EXPECT_TRUE(pwg_raster_config.LoadFrom(description));

  EXPECT_TRUE(content_types.Contains("image/pwg-raster"));
  EXPECT_TRUE(content_types.Contains("image/jpeg"));

  EXPECT_EQ(0U, pwg_raster_config.value().document_types_supported.size());
  EXPECT_EQ(DocumentSheetBack::MANUAL_TUMBLE,
            pwg_raster_config.value().document_sheet_back);
  EXPECT_TRUE(pwg_raster_config.value().reverse_order_streaming);
  EXPECT_FALSE(pwg_raster_config.value().rotate_all_pages);

  EXPECT_TRUE(color.Contains(Color(ColorType::STANDARD_COLOR)));
  EXPECT_TRUE(color.Contains(Color(ColorType::STANDARD_MONOCHROME)));
  Color custom(ColorType::CUSTOM_MONOCHROME);
  custom.vendor_id = "123";
  custom.custom_display_name = "monochrome";
  EXPECT_TRUE(color.Contains(custom));
  EXPECT_EQ(Color(ColorType::STANDARD_COLOR), color.GetDefault());

  EXPECT_TRUE(duplex.Contains(DuplexType::LONG_EDGE));
  EXPECT_TRUE(duplex.Contains(DuplexType::SHORT_EDGE));
  EXPECT_TRUE(duplex.Contains(DuplexType::NO_DUPLEX));
  EXPECT_EQ(DuplexType::LONG_EDGE, duplex.GetDefault());

  EXPECT_TRUE(orientation.Contains(OrientationType::PORTRAIT));
  EXPECT_TRUE(orientation.Contains(OrientationType::LANDSCAPE));
  EXPECT_TRUE(orientation.Contains(OrientationType::AUTO_ORIENTATION));
  EXPECT_EQ(OrientationType::AUTO_ORIENTATION, orientation.GetDefault());

  EXPECT_TRUE(margins.Contains(Margins(MarginsType::NO_MARGINS, 0, 0, 0, 0)));
  EXPECT_TRUE(margins.Contains(
      Margins(MarginsType::STANDARD_MARGINS, 100, 200, 300, 400)));
  EXPECT_TRUE(
      margins.Contains(Margins(MarginsType::CUSTOM_MARGINS, 1, 2, 3, 4)));
  EXPECT_EQ(Margins(MarginsType::NO_MARGINS, 0, 0, 0, 0), margins.GetDefault());

  EXPECT_TRUE(dpi.Contains(Dpi(150, 250)));
  EXPECT_TRUE(dpi.Contains(Dpi(600, 1600)));
  EXPECT_EQ(Dpi(600, 1600), dpi.GetDefault());

  EXPECT_TRUE(fit_to_page.Contains(FitToPageType::NO_FITTING));
  EXPECT_TRUE(fit_to_page.Contains(FitToPageType::FIT_TO_PAGE));
  EXPECT_TRUE(fit_to_page.Contains(FitToPageType::GROW_TO_PAGE));
  EXPECT_TRUE(fit_to_page.Contains(FitToPageType::SHRINK_TO_PAGE));
  EXPECT_TRUE(fit_to_page.Contains(FitToPageType::FILL_PAGE));
  EXPECT_EQ(FitToPageType::NO_FITTING, fit_to_page.GetDefault());

  Media default_media = MediaBuilder()
                            .WithStandardName(MediaSize::NA_LETTER)
                            .WithSizeAndDefaultPrintableArea({2222, 3333})
                            .Build();
  EXPECT_TRUE(media.Contains(default_media));
  EXPECT_TRUE(media.Contains(MediaBuilder()
                                 .WithStandardName(MediaSize::ISO_A6)
                                 .WithSizeAndDefaultPrintableArea({4444, 5555})
                                 .Build()));
  EXPECT_TRUE(media.Contains(MediaBuilder()
                                 .WithStandardName(MediaSize::JPN_YOU4)
                                 .WithSizeAndDefaultPrintableArea({6666, 7777})
                                 .Build()));
  EXPECT_TRUE(media.Contains(MediaBuilder()
                                 .WithCustomName("Feed", "FEED")
                                 .WithSizeAndDefaultPrintableArea({1111, 2222})
                                 .WithMaxHeight(9999)
                                 .Build()));
  EXPECT_EQ(default_media, media.GetDefault());

  EXPECT_TRUE(media_type.Contains(MediaType("stationery", "Plain Paper")));
  EXPECT_TRUE(media_type.Contains(MediaType("photographic", "Photo Paper")));

  EXPECT_FALSE(collate.default_value());
  EXPECT_TRUE(reverse.default_value());

  EXPECT_EQ(NormalizeJson(kCdd),
            NormalizeJson(description.ToStringForTesting()));
}

TEST(PrinterDescriptionTest, CjtInit) {
  CloudDeviceDescription description;
  EXPECT_EQ(NormalizeJson(kDefaultCjt),
            NormalizeJson(description.ToStringForTesting()));

  PwgRasterConfigTicketItem pwg_raster_config;
  VendorTicketItems vendor_items;
  ColorTicketItem color;
  DuplexTicketItem duplex;
  OrientationTicketItem orientation;
  MarginsTicketItem margins;
  DpiTicketItem dpi;
  FitToPageTicketItem fit_to_page;
  MediaTicketItem media;
  CopiesTicketItem copies;
  PageRangeTicketItem page_range;
  CollateTicketItem collate;
  ReverseTicketItem reverse;

  EXPECT_FALSE(pwg_raster_config.LoadFrom(description));
  EXPECT_FALSE(vendor_items.LoadFrom(description));
  EXPECT_FALSE(color.LoadFrom(description));
  EXPECT_FALSE(duplex.LoadFrom(description));
  EXPECT_FALSE(orientation.LoadFrom(description));
  EXPECT_FALSE(copies.LoadFrom(description));
  EXPECT_FALSE(margins.LoadFrom(description));
  EXPECT_FALSE(dpi.LoadFrom(description));
  EXPECT_FALSE(fit_to_page.LoadFrom(description));
  EXPECT_FALSE(page_range.LoadFrom(description));
  EXPECT_FALSE(media.LoadFrom(description));
  EXPECT_FALSE(collate.LoadFrom(description));
  EXPECT_FALSE(reverse.LoadFrom(description));
  EXPECT_FALSE(media.LoadFrom(description));
}

TEST(PrinterDescriptionTest, CjtInvalid) {
  CloudDeviceDescription ticket;
  EXPECT_FALSE(ticket.InitFromString(kBadVersionCjt));
}

TEST(PrinterDescriptionTest, CjtSetAll) {
  CloudDeviceDescription description;

  PwgRasterConfigTicketItem pwg_raster_config;
  VendorTicketItems vendor_items;
  ColorTicketItem color;
  DuplexTicketItem duplex;
  OrientationTicketItem orientation;
  MarginsTicketItem margins;
  DpiTicketItem dpi;
  FitToPageTicketItem fit_to_page;
  MediaTicketItem media;
  CopiesTicketItem copies;
  PageRangeTicketItem page_range;
  CollateTicketItem collate;
  ReverseTicketItem reverse;

  PwgRasterConfig custom_raster;
  custom_raster.document_sheet_back = DocumentSheetBack::MANUAL_TUMBLE;
  custom_raster.reverse_order_streaming = true;
  custom_raster.rotate_all_pages = false;
  pwg_raster_config.set_value(custom_raster);
  VendorItem label_cutter("finishings", "trim");
  vendor_items.AddOption(std::move(label_cutter));
  color.set_value(Color(ColorType::STANDARD_MONOCHROME));
  duplex.set_value(DuplexType::NO_DUPLEX);
  orientation.set_value(OrientationType::LANDSCAPE);
  copies.set_value(123);
  margins.set_value(Margins(MarginsType::CUSTOM_MARGINS, 7, 6, 3, 1));
  dpi.set_value(Dpi(562, 125));
  fit_to_page.set_value(FitToPageType::SHRINK_TO_PAGE);
  PageRange page_ranges;
  page_ranges.push_back(Interval(1, 99));
  page_ranges.push_back(Interval(150));
  page_range.set_value(page_ranges);
  media.set_value(
      MediaBuilder()
          .WithStandardName(MediaSize::ISO_C7C6)
          .WithSizeAndPrintableArea({4261, 334}, {300, 100, 3661, 134})
          .Build());
  collate.set_value(false);
  reverse.set_value(true);

  pwg_raster_config.SaveTo(&description);
  vendor_items.SaveTo(&description);
  color.SaveTo(&description);
  duplex.SaveTo(&description);
  orientation.SaveTo(&description);
  copies.SaveTo(&description);
  margins.SaveTo(&description);
  dpi.SaveTo(&description);
  fit_to_page.SaveTo(&description);
  page_range.SaveTo(&description);
  media.SaveTo(&description);
  collate.SaveTo(&description);
  reverse.SaveTo(&description);

  EXPECT_EQ(NormalizeJson(kCjt),
            NormalizeJson(description.ToStringForTesting()));
}

TEST(PrinterDescriptionTest, CjtGetAll) {
  CloudDeviceDescription description;
  ASSERT_TRUE(description.InitFromString(kCjt));

  VendorTicketItems vendor_items;
  ColorTicketItem color;
  DuplexTicketItem duplex;
  OrientationTicketItem orientation;
  MarginsTicketItem margins;
  DpiTicketItem dpi;
  FitToPageTicketItem fit_to_page;
  MediaTicketItem media;
  CopiesTicketItem copies;
  PageRangeTicketItem page_range;
  CollateTicketItem collate;
  ReverseTicketItem reverse;
  PwgRasterConfigTicketItem pwg_raster_config;

  EXPECT_TRUE(pwg_raster_config.LoadFrom(description));
  EXPECT_TRUE(vendor_items.LoadFrom(description));
  EXPECT_TRUE(color.LoadFrom(description));
  EXPECT_TRUE(duplex.LoadFrom(description));
  EXPECT_TRUE(orientation.LoadFrom(description));
  EXPECT_TRUE(copies.LoadFrom(description));
  EXPECT_TRUE(margins.LoadFrom(description));
  EXPECT_TRUE(dpi.LoadFrom(description));
  EXPECT_TRUE(fit_to_page.LoadFrom(description));
  EXPECT_TRUE(page_range.LoadFrom(description));
  EXPECT_TRUE(media.LoadFrom(description));
  EXPECT_TRUE(collate.LoadFrom(description));
  EXPECT_TRUE(reverse.LoadFrom(description));
  EXPECT_TRUE(media.LoadFrom(description));

  EXPECT_EQ(DocumentSheetBack::MANUAL_TUMBLE,
            pwg_raster_config.value().document_sheet_back);
  EXPECT_TRUE(pwg_raster_config.value().reverse_order_streaming);
  EXPECT_FALSE(pwg_raster_config.value().rotate_all_pages);
  ASSERT_EQ(vendor_items.size(), 1u);
  EXPECT_EQ(vendor_items[0].id, "finishings");
  EXPECT_EQ(vendor_items[0].value, "trim");
  EXPECT_EQ(color.value(), Color(ColorType::STANDARD_MONOCHROME));
  EXPECT_EQ(duplex.value(), DuplexType::NO_DUPLEX);
  EXPECT_EQ(orientation.value(), OrientationType::LANDSCAPE);
  EXPECT_EQ(copies.value(), 123);
  EXPECT_EQ(margins.value(), Margins(MarginsType::CUSTOM_MARGINS, 7, 6, 3, 1));
  EXPECT_EQ(dpi.value(), Dpi(562, 125));
  EXPECT_EQ(fit_to_page.value(), FitToPageType::SHRINK_TO_PAGE);
  PageRange page_ranges;
  page_ranges.push_back(Interval(1, 99));
  page_ranges.push_back(Interval(150));
  EXPECT_EQ(page_range.value(), page_ranges);
  EXPECT_EQ(media.value(),
            MediaBuilder()
                .WithStandardName(MediaSize::ISO_C7C6)
                .WithSizeAndPrintableArea({4261, 334}, {300, 100, 3661, 134})
                .Build());
  EXPECT_FALSE(collate.value());
  EXPECT_TRUE(reverse.value());

  EXPECT_EQ(NormalizeJson(kCjt),
            NormalizeJson(description.ToStringForTesting()));
}

TEST(PrinterDescriptionTest, ContentTypesCapabilityIterator) {
  ContentTypesCapability content_types;

  base::flat_set<ContentType> expected_types{"type1", "type2", "type3"};
  for (ContentType type : expected_types) {
    content_types.AddOption(std::move(type));
  }

  for (const auto& content_type : content_types) {
    EXPECT_EQ(expected_types.erase(content_type), 1u);
  }
}

TEST(PrinterDescriptionMediaBuilderTest, StandardName) {
  Media media_a1 = MediaBuilder()
                       .WithStandardName(MediaSize::ISO_A1)
                       .WithSizeAndDefaultPrintableArea({100, 200})
                       .Build();

  // Note that `Media::operator==` does not check all fields.
  EXPECT_EQ(MediaSize::ISO_A1, media_a1.size_name);
  EXPECT_EQ(gfx::Size(100, 200), media_a1.size_um);
  EXPECT_FALSE(media_a1.is_continuous_feed);
  EXPECT_TRUE(media_a1.custom_display_name.empty());
  EXPECT_TRUE(media_a1.vendor_id.empty());
  EXPECT_EQ(gfx::Rect(100, 200), media_a1.printable_area_um);
  EXPECT_EQ(0, media_a1.max_height_um);
  EXPECT_TRUE(media_a1.IsValid());

  Media media_a4_with_printable_area =
      MediaBuilder()
          .WithStandardName(MediaSize::ISO_A4)
          .WithSizeAndPrintableArea({100, 200}, {5, 6, 50, 60})
          .Build();

  EXPECT_EQ(MediaSize::ISO_A4, media_a4_with_printable_area.size_name);
  EXPECT_EQ(gfx::Size(100, 200), media_a4_with_printable_area.size_um);
  EXPECT_FALSE(media_a4_with_printable_area.is_continuous_feed);
  EXPECT_TRUE(media_a4_with_printable_area.custom_display_name.empty());
  EXPECT_TRUE(media_a4_with_printable_area.vendor_id.empty());
  EXPECT_EQ(gfx::Rect(5, 6, 50, 60),
            media_a4_with_printable_area.printable_area_um);
  EXPECT_EQ(0, media_a4_with_printable_area.max_height_um);
  EXPECT_TRUE(media_a4_with_printable_area.IsValid());
}

TEST(PrinterDescriptionMediaBuilderTest, CustomName) {
  Media media_custom1 =
      MediaBuilder()
          .WithCustomName("name", "id")
          .WithSizeAndPrintableArea({2000, 2500}, {100, 150, 1800, 2000})
          .Build();

  EXPECT_EQ(MediaSize::CUSTOM_MEDIA, media_custom1.size_name);
  EXPECT_EQ(gfx::Size(2000, 2500), media_custom1.size_um);
  EXPECT_FALSE(media_custom1.is_continuous_feed);
  EXPECT_EQ("name", media_custom1.custom_display_name);
  EXPECT_EQ("id", media_custom1.vendor_id);
  EXPECT_EQ(gfx::Rect(100, 150, 1800, 2000), media_custom1.printable_area_um);
  EXPECT_EQ(0, media_custom1.max_height_um);
  EXPECT_TRUE(media_custom1.IsValid());

  Media media_custom2 =
      MediaBuilder()
          .WithCustomName("name2", "")
          .WithSizeAndPrintableArea({500, 300}, {50, 60, 120, 200})
          .Build();

  EXPECT_EQ(MediaSize::CUSTOM_MEDIA, media_custom2.size_name);
  EXPECT_EQ(gfx::Size(500, 300), media_custom2.size_um);
  EXPECT_FALSE(media_custom2.is_continuous_feed);
  EXPECT_EQ("name2", media_custom2.custom_display_name);
  EXPECT_TRUE(media_custom2.vendor_id.empty());
  EXPECT_EQ(gfx::Rect(50, 60, 120, 200), media_custom2.printable_area_um);
  EXPECT_EQ(0, media_custom2.max_height_um);
  EXPECT_TRUE(media_custom2.IsValid());
}

TEST(PrinterDescriptionMediaBuilderTest, EmptySize) {
  Media media_empty_size = MediaBuilder()
                               .WithStandardName(MediaSize::NA_LETTER)
                               .WithSizeAndDefaultPrintableArea({0, 0})
                               .Build();

  EXPECT_EQ(MediaSize::NA_LETTER, media_empty_size.size_name);
  EXPECT_EQ(gfx::Size(0, 0), media_empty_size.size_um);
  EXPECT_FALSE(media_empty_size.is_continuous_feed);
  EXPECT_TRUE(media_empty_size.custom_display_name.empty());
  EXPECT_TRUE(media_empty_size.vendor_id.empty());
  EXPECT_EQ(gfx::Rect(0, 0), media_empty_size.printable_area_um);
  EXPECT_EQ(0, media_empty_size.max_height_um);
  EXPECT_FALSE(media_empty_size.IsValid());

  Media media_no_size =
      MediaBuilder().WithStandardName(MediaSize::NA_LEGAL).Build();

  EXPECT_EQ(MediaSize::NA_LEGAL, media_no_size.size_name);
  EXPECT_EQ(gfx::Size(0, 0), media_no_size.size_um);
  EXPECT_FALSE(media_no_size.is_continuous_feed);
  EXPECT_TRUE(media_no_size.custom_display_name.empty());
  EXPECT_TRUE(media_no_size.vendor_id.empty());
  EXPECT_EQ(gfx::Rect(0, 0), media_no_size.printable_area_um);
  EXPECT_EQ(0, media_no_size.max_height_um);
  EXPECT_FALSE(media_no_size.IsValid());
}

TEST(PrinterDescriptionMediaBuilderTest, EmptyWidth) {
  Media media_empty_width = MediaBuilder()
                                .WithStandardName(MediaSize::NA_LETTER)
                                .WithSizeAndDefaultPrintableArea({0, 100})
                                .Build();

  EXPECT_EQ(MediaSize::NA_LETTER, media_empty_width.size_name);
  EXPECT_EQ(gfx::Size(0, 100), media_empty_width.size_um);
  EXPECT_FALSE(media_empty_width.is_continuous_feed);
  EXPECT_TRUE(media_empty_width.custom_display_name.empty());
  EXPECT_TRUE(media_empty_width.vendor_id.empty());
  EXPECT_EQ(gfx::Rect(0, 100), media_empty_width.printable_area_um);
  EXPECT_EQ(0, media_empty_width.max_height_um);
  EXPECT_FALSE(media_empty_width.IsValid());
}

TEST(PrinterDescriptionMediaBuilderTest, ContinuousFeedHeight) {
  Media media_continuous_height =
      MediaBuilder()
          .WithCustomName("FEED", "feed")
          .WithSizeAndDefaultPrintableArea({100, 200})
          .WithMaxHeight(500)
          .Build();

  EXPECT_EQ(MediaSize::CUSTOM_MEDIA, media_continuous_height.size_name);
  EXPECT_EQ(gfx::Size(100, 200), media_continuous_height.size_um);
  EXPECT_TRUE(media_continuous_height.is_continuous_feed);
  EXPECT_EQ("FEED", media_continuous_height.custom_display_name);
  EXPECT_EQ("feed", media_continuous_height.vendor_id);
  EXPECT_EQ(gfx::Rect(100, 200), media_continuous_height.printable_area_um);
  EXPECT_EQ(500, media_continuous_height.max_height_um);
  EXPECT_TRUE(media_continuous_height.IsValid());
}

TEST(PrinterDescriptionMediaBuilderTest, WithNameMaybeBasedOnSize) {
  Media media_letter =
      MediaBuilder()
          .WithSizeAndDefaultPrintableArea({215900, 279400})
          .WithNameMaybeBasedOnSize(/*custom_display_name=*/"custom_letter",
                                    /*vendor_id=*/"vendor_letter")
          .Build();

  EXPECT_EQ(MediaSize::NA_LETTER, media_letter.size_name);
  EXPECT_EQ(gfx::Size(215900, 279400), media_letter.size_um);
  EXPECT_FALSE(media_letter.is_continuous_feed);
  EXPECT_EQ("custom_letter", media_letter.custom_display_name);
  EXPECT_EQ("vendor_letter", media_letter.vendor_id);
  EXPECT_EQ(gfx::Rect(215900, 279400), media_letter.printable_area_um);
  EXPECT_EQ(0, media_letter.max_height_um);
  EXPECT_TRUE(media_letter.IsValid());

  Media media_non_standard =
      MediaBuilder()
          .WithSizeAndDefaultPrintableArea({123000, 456000})
          .WithNameMaybeBasedOnSize(/*custom_display_name=*/"123x456",
                                    /*vendor_id=*/"vendor_123x456")
          .Build();

  EXPECT_EQ(MediaSize::CUSTOM_MEDIA, media_non_standard.size_name);
  EXPECT_EQ(gfx::Size(123000, 456000), media_non_standard.size_um);
  EXPECT_FALSE(media_non_standard.is_continuous_feed);
  EXPECT_EQ("123x456", media_non_standard.custom_display_name);
  EXPECT_EQ("vendor_123x456", media_non_standard.vendor_id);
  EXPECT_EQ(gfx::Rect(123000, 456000), media_non_standard.printable_area_um);
  EXPECT_EQ(0, media_non_standard.max_height_um);
  EXPECT_TRUE(media_non_standard.IsValid());
}

TEST(PrinterDescriptionMediaBuilderTest,
     WithSizeAndPrintableAreaBasedOnStandardName) {
  Media media = MediaBuilder()
                    .WithStandardName(MediaSize::ISO_A3)
                    .WithSizeAndPrintableAreaBasedOnStandardName()
                    .Build();

  EXPECT_EQ(MediaSize::ISO_A3, media.size_name);
  EXPECT_EQ(gfx::Size(297000, 420000), media.size_um);
  EXPECT_FALSE(media.is_continuous_feed);
  EXPECT_TRUE(media.custom_display_name.empty());
  EXPECT_TRUE(media.vendor_id.empty());
  EXPECT_EQ(gfx::Rect(297000, 420000), media.printable_area_um);
  EXPECT_EQ(0, media.max_height_um);
  EXPECT_TRUE(media.IsValid());
}

TEST(PrinterDescriptionMediaBuilderTest, MultipleBuilds) {
  MediaBuilder builder;
  Media media1 = builder.WithStandardName(MediaSize::NA_LETTER)
                     .WithSizeAndDefaultPrintableArea({100, 200})
                     .Build();
  Media media2 = builder.Build();

  EXPECT_EQ(MediaSize::NA_LETTER, media1.size_name);
  EXPECT_EQ(gfx::Size(100, 200), media1.size_um);
  EXPECT_FALSE(media1.is_continuous_feed);
  EXPECT_TRUE(media1.custom_display_name.empty());
  EXPECT_TRUE(media1.vendor_id.empty());
  EXPECT_EQ(gfx::Rect(100, 200), media1.printable_area_um);
  EXPECT_EQ(0, media1.max_height_um);
  EXPECT_TRUE(media1.IsValid());

  EXPECT_EQ(media1.size_name, media2.size_name);
  EXPECT_EQ(media1.size_um, media2.size_um);
  EXPECT_EQ(media1.is_continuous_feed, media2.is_continuous_feed);
  EXPECT_EQ(media1.custom_display_name, media2.custom_display_name);
  EXPECT_EQ(media1.vendor_id, media2.vendor_id);
  EXPECT_EQ(media1.printable_area_um, media2.printable_area_um);
  EXPECT_EQ(media1.max_height_um, media2.max_height_um);
  EXPECT_TRUE(media2.IsValid());
}

}  // namespace printer

}  // namespace cloud_devices
