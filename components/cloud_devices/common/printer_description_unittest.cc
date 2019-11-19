// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/printer_description.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cloud_devices {

namespace printer {

// Makes sure that same JSON value represented by same strings to simplify
// comparison.
std::string NormalizeJson(const std::string& json) {
  std::string result;
  base::JSONWriter::Write(*base::JSONReader::Read(json), &result);
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
            "is_default": true,
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
            "is_default": true,
            "name": "NA_LETTER",
            "width_microns": 2222,
            "height_microns": 3333
          }, {
            "name": "ISO_A6",
            "width_microns": 4444,
            "height_microns": 5555
          }, {
            "name": "JPN_YOU4",
            "width_microns": 6666,
            "height_microns": 7777
          }, {
            "width_microns": 1111,
            "is_continuous_feed": true,
            "custom_display_name": "Feed",
            "vendor_id": "FEED"
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

const char kNoDefaultCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "color": {
          "option": [ {
            "type": "STANDARD_COLOR"
          }, {
            "type": "STANDARD_MONOCHROME"
          } ]
        }
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
        "value": "value",
        "display_name": "name"
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

#if defined(OS_CHROMEOS)
const char kPinOnlyCdd[] = R"(
    {
      "version": "1.0",
      "printer": {
        "pin": {
          "supported": true
        }
      }
    })";
#endif  // defined(OS_CHROMEOS)

const char kCjt[] = R"(
    {
      "version": "1.0",
      "print": {
        "pwg_raster_config": {
          "document_sheet_back": "MANUAL_TUMBLE",
          "reverse_order_streaming": true
        },
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
  EXPECT_EQ(NormalizeJson(kDefaultCdd), NormalizeJson(description.ToString()));

  ContentTypesCapability content_types;
  PwgRasterConfigCapability pwg_raster;
  ColorCapability color;
  DuplexCapability duplex;
  OrientationCapability orientation;
  MarginsCapability margins;
  DpiCapability dpi;
  FitToPageCapability fit_to_page;
  MediaCapability media;
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
  EXPECT_FALSE(collate.LoadFrom(description));
  EXPECT_FALSE(reverse.LoadFrom(description));
  EXPECT_FALSE(media.LoadFrom(description));
}

TEST(PrinterDescriptionTest, CddInvalid) {
  CloudDeviceDescription description;
  ColorCapability color;

  EXPECT_FALSE(description.InitFromString(kBadVersionCdd));

  EXPECT_TRUE(description.InitFromString(kNoDefaultCdd));
  EXPECT_FALSE(color.LoadFrom(description));

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

  color.AddDefaultOption(Color(ColorType::STANDARD_COLOR), true);
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

  media.AddDefaultOption(Media(MediaType::NA_LETTER, 2222, 3333), true);
  media.AddOption(Media(MediaType::ISO_A6, 4444, 5555));
  media.AddOption(Media(MediaType::JPN_YOU4, 6666, 7777));
  media.AddOption(Media("Feed", "FEED", 1111, 0));

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
  collate.SaveTo(&description);
  reverse.SaveTo(&description);
  pwg_raster_config.SaveTo(&description);

  EXPECT_EQ(NormalizeJson(kCdd), NormalizeJson(description.ToString()));
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
              NormalizeJson(description.ToString()));
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
              NormalizeJson(description.ToString()));
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
              NormalizeJson(description.ToString()));
  }
  {
    CloudDeviceDescription description;

    PwgRasterConfig custom_raster;
    custom_raster.document_sheet_back = DocumentSheetBack::ROTATED;

    PwgRasterConfigCapability pwg_raster;
    pwg_raster.set_value(custom_raster);
    pwg_raster.SaveTo(&description);

    EXPECT_EQ(NormalizeJson(kDocumentTypeNoneCdd),
              NormalizeJson(description.ToString()));
  }
}

TEST(PrinterDescriptionTest, CddGetRangeVendorCapability) {
  for (const auto& capacity : kTestRangeCapabilities) {
    base::Optional<base::Value> value =
        base::JSONReader::Read(capacity.json_name);
    ASSERT_TRUE(value);
    base::Value description = std::move(*value);
    RangeVendorCapability range_capability;
    EXPECT_TRUE(range_capability.LoadFrom(description));
    EXPECT_EQ(capacity.range_capability, range_capability);
  }

  const char* const kInvalidJsonNames[] = {
      kMissingMinValueRangeVendorCapabilityJson,
      kInvalidTypeRangeVendorCapabilityJson,
      kInvalidBoundariesRangeVendorCapabilityJson,
      kInvalidDefaultValueRangeVendorCapabilityJson};
  for (const char* invalid_json_name : kInvalidJsonNames) {
    base::Optional<base::Value> value =
        base::JSONReader::Read(invalid_json_name);
    ASSERT_TRUE(value);
    base::Value description = std::move(*value);
    RangeVendorCapability range_capability;
    EXPECT_FALSE(range_capability.LoadFrom(description));
  }
}

TEST(PrinterDescriptionTest, CddSetRangeVendorCapability) {
  for (const auto& capacity : kTestRangeCapabilities) {
    base::Value range_capability_value(base::Value::Type::DICTIONARY);
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
    base::Optional<base::Value> value =
        base::JSONReader::Read(kSelectVendorCapabilityJson);
    ASSERT_TRUE(value);
    base::Value description = std::move(*value);
    SelectVendorCapability select_capability;
    EXPECT_TRUE(select_capability.LoadFrom(description));
    EXPECT_EQ(2u, select_capability.size());
    EXPECT_TRUE(select_capability.Contains(
        SelectVendorCapabilityOption("value_1", "name_1")));
    EXPECT_TRUE(select_capability.Contains(
        SelectVendorCapabilityOption("value_2", "name_2")));
    EXPECT_EQ(SelectVendorCapabilityOption("value_2", "name_2"),
              select_capability.GetDefault());
  }

  const char* const kInvalidJsonNames[] = {
      kNotListSelectVendorCapabilityJson,
      kMissingValueSelectVendorCapabilityJson,
      kMissingDisplayNameSelectVendorCapabilityJson,
      kNoDefaultSelectVendorCapabilityJson,
      kSeveralDefaultsSelectVendorCapabilityJson};
  for (const char* invalid_json_name : kInvalidJsonNames) {
    base::Optional<base::Value> value =
        base::JSONReader::Read(invalid_json_name);
    ASSERT_TRUE(value);
    base::Value description = std::move(*value);
    SelectVendorCapability select_capability;
    EXPECT_FALSE(select_capability.LoadFrom(description));
  }
}

TEST(PrinterDescriptionTest, CddSetSelectVendorCapability) {
  SelectVendorCapability select_capability;
  select_capability.AddOption(
      SelectVendorCapabilityOption("value_1", "name_1"));
  select_capability.AddDefaultOption(
      SelectVendorCapabilityOption("value_2", "name_2"), true);
  base::Value select_capability_value(base::Value::Type::DICTIONARY);
  select_capability.SaveTo(&select_capability_value);
  std::string select_capability_str;
  EXPECT_TRUE(base::JSONWriter::WriteWithOptions(
      select_capability_value, base::JSONWriter::OPTIONS_PRETTY_PRINT,
      &select_capability_str));
  EXPECT_EQ(NormalizeJson(kSelectVendorCapabilityJson),
            NormalizeJson(select_capability_str));
}

TEST(PrinterDescriptionTest, CddGetTypedValueVendorCapability) {
  for (const auto& capacity : kTestTypedValueCapabilities) {
    base::Optional<base::Value> value =
        base::JSONReader::Read(capacity.json_name);
    ASSERT_TRUE(value);
    base::Value description = std::move(*value);
    TypedValueVendorCapability typed_value_capability;
    EXPECT_TRUE(typed_value_capability.LoadFrom(description));
    EXPECT_EQ(capacity.typed_value_capability, typed_value_capability);
  }

  const char* const kInvalidJsonNames[] = {
      kMissingValueTypeTypedValueVendorCapabilityJson,
      kInvalidBooleanTypedValueVendorCapabilityJson,
      kInvalidFloatTypedValueVendorCapabilityJson,
      kInvalidIntegerTypedValueVendorCapabilityJson};
  for (const char* invalid_json_name : kInvalidJsonNames) {
    base::Optional<base::Value> value =
        base::JSONReader::Read(invalid_json_name);
    ASSERT_TRUE(value);
    base::Value description = std::move(*value);
    TypedValueVendorCapability typed_value_capability;
    EXPECT_FALSE(typed_value_capability.LoadFrom(description));
  }
}

TEST(PrinterDescriptionTest, CddSetTypedValueVendorCapability) {
  for (const auto& capacity : kTestTypedValueCapabilities) {
    base::Value typed_value_capability_value(base::Value::Type::DICTIONARY);
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
            NormalizeJson(description.ToString()));
}

#if defined(OS_CHROMEOS)
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
  EXPECT_EQ(NormalizeJson(kPinOnlyCdd), NormalizeJson(description.ToString()));
}
#endif  // defined(OS_CHROMEOS)

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
  EXPECT_TRUE(collate.LoadFrom(description));
  EXPECT_TRUE(reverse.LoadFrom(description));
  EXPECT_TRUE(media.LoadFrom(description));
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

  EXPECT_TRUE(media.Contains(Media(MediaType::NA_LETTER, 2222, 3333)));
  EXPECT_TRUE(media.Contains(Media(MediaType::ISO_A6, 4444, 5555)));
  EXPECT_TRUE(media.Contains(Media(MediaType::JPN_YOU4, 6666, 7777)));
  EXPECT_TRUE(media.Contains(Media("Feed", "FEED", 1111, 0)));
  EXPECT_EQ(Media(MediaType::NA_LETTER, 2222, 3333), media.GetDefault());

  EXPECT_FALSE(collate.default_value());
  EXPECT_TRUE(reverse.default_value());

  EXPECT_EQ(NormalizeJson(kCdd), NormalizeJson(description.ToString()));
}

TEST(PrinterDescriptionTest, CjtInit) {
  CloudDeviceDescription description;
  EXPECT_EQ(NormalizeJson(kDefaultCjt), NormalizeJson(description.ToString()));

  PwgRasterConfigTicketItem pwg_raster_config;
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
  media.set_value(Media(MediaType::ISO_C7C6, 4261, 334));
  collate.set_value(false);
  reverse.set_value(true);

  pwg_raster_config.SaveTo(&description);
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

  EXPECT_EQ(NormalizeJson(kCjt), NormalizeJson(description.ToString()));
}

TEST(PrinterDescriptionTest, CjtGetAll) {
  CloudDeviceDescription description;
  ASSERT_TRUE(description.InitFromString(kCjt));

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
  EXPECT_EQ(media.value(), Media(MediaType::ISO_C7C6, 4261, 334));
  EXPECT_FALSE(collate.value());
  EXPECT_TRUE(reverse.value());

  EXPECT_EQ(NormalizeJson(kCjt), NormalizeJson(description.ToString()));
}

}  // namespace printer

}  // namespace cloud_devices
