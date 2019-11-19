// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/printer_description.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/cloud_devices/common/cloud_device_description_consts.h"
#include "components/cloud_devices/common/description_items_inl.h"

namespace cloud_devices {

namespace printer {

namespace {

const int32_t kMaxPageNumber = 1000000;

const char kSectionPrint[] = "print";
const char kSectionPrinter[] = "printer";

const char kKeyCustomDisplayName[] = "custom_display_name";
const char kKeyContentType[] = "content_type";
const char kKeyDisplayName[] = "display_name";
const char kKeyId[] = "id";
const char kKeyName[] = "name";
const char kKeyType[] = "type";
const char kKeyValue[] = "value";
const char kKeyValueType[] = "value_type";
const char kKeyVendorId[] = "vendor_id";

// extern is required to be used in templates.
extern const char kOptionCollate[] = "collate";
extern const char kOptionColor[] = "color";
extern const char kOptionContentType[] = "supported_content_type";
extern const char kOptionCopies[] = "copies";
extern const char kOptionDpi[] = "dpi";
extern const char kOptionDuplex[] = "duplex";
extern const char kOptionFitToPage[] = "fit_to_page";
extern const char kOptionMargins[] = "margins";
extern const char kOptionMediaSize[] = "media_size";
extern const char kOptionPageOrientation[] = "page_orientation";
extern const char kOptionPageRange[] = "page_range";
extern const char kOptionReverse[] = "reverse_order";
extern const char kOptionPwgRasterConfig[] = "pwg_raster_config";
extern const char kOptionRangeCapability[] = "range_cap";
extern const char kOptionSelectCapability[] = "select_cap";
extern const char kOptionTypedValueCapability[] = "typed_value_cap";
extern const char kOptionVendorCapability[] = "vendor_capability";
#if defined(OS_CHROMEOS)
extern const char kOptionPin[] = "pin";
#endif  // defined(OS_CHROMEOS)

const char kMarginBottom[] = "bottom_microns";
const char kMarginLeft[] = "left_microns";
const char kMarginRight[] = "right_microns";
const char kMarginTop[] = "top_microns";

const char kDpiHorizontal[] = "horizontal_dpi";
const char kDpiVertical[] = "vertical_dpi";

const char kMediaWidth[] = "width_microns";
const char kMediaHeight[] = "height_microns";
const char kMediaIsContinuous[] = "is_continuous_feed";

const char kPageRangeInterval[] = "interval";
const char kPageRangeEnd[] = "end";
const char kPageRangeStart[] = "start";

const char kPwgRasterDocumentTypeSupported[] = "document_type_supported";
const char kPwgRasterDocumentSheetBack[] = "document_sheet_back";
const char kPwgRasterReverseOrderStreaming[] = "reverse_order_streaming";
const char kPwgRasterRotateAllPages[] = "rotate_all_pages";

const char kVendorCapabilityMinValue[] = "min";
const char kVendorCapabilityMaxValue[] = "max";
const char kVendorCapabilityDefaultValue[] = "default";

#if defined(OS_CHROMEOS)
const char kPinSupported[] = "supported";
#endif  // defined(OS_CHROMEOS)

const char kTypeRangeVendorCapabilityFloat[] = "FLOAT";
const char kTypeRangeVendorCapabilityInteger[] = "INTEGER";

const char kTypeTypedValueVendorCapabilityBoolean[] = "BOOLEAN";
const char kTypeTypedValueVendorCapabilityFloat[] = "FLOAT";
const char kTypeTypedValueVendorCapabilityInteger[] = "INTEGER";
const char kTypeTypedValueVendorCapabilityString[] = "STRING";

const char kTypeVendorCapabilityRange[] = "RANGE";
const char kTypeVendorCapabilitySelect[] = "SELECT";
const char kTypeVendorCapabilityTypedValue[] = "TYPED_VALUE";

const char kTypeColorColor[] = "STANDARD_COLOR";
const char kTypeColorMonochrome[] = "STANDARD_MONOCHROME";
const char kTypeColorCustomColor[] = "CUSTOM_COLOR";
const char kTypeColorCustomMonochrome[] = "CUSTOM_MONOCHROME";
const char kTypeColorAuto[] = "AUTO";

const char kTypeDuplexLongEdge[] = "LONG_EDGE";
const char kTypeDuplexNoDuplex[] = "NO_DUPLEX";
const char kTypeDuplexShortEdge[] = "SHORT_EDGE";

const char kTypeFitToPageFillPage[] = "FILL_PAGE";
const char kTypeFitToPageFitToPage[] = "FIT_TO_PAGE";
const char kTypeFitToPageGrowToPage[] = "GROW_TO_PAGE";
const char kTypeFitToPageNoFitting[] = "NO_FITTING";
const char kTypeFitToPageShrinkToPage[] = "SHRINK_TO_PAGE";

const char kTypeMarginsBorderless[] = "BORDERLESS";
const char kTypeMarginsCustom[] = "CUSTOM";
const char kTypeMarginsStandard[] = "STANDARD";
const char kTypeOrientationAuto[] = "AUTO";

const char kTypeOrientationLandscape[] = "LANDSCAPE";
const char kTypeOrientationPortrait[] = "PORTRAIT";

const char kTypeDocumentSupportedTypeSRGB8[] = "SRGB_8";
const char kTypeDocumentSupportedTypeSGRAY8[] = "SGRAY_8";

const char kTypeDocumentSheetBackNormal[] = "NORMAL";
const char kTypeDocumentSheetBackRotated[] = "ROTATED";
const char kTypeDocumentSheetBackManualTumble[] = "MANUAL_TUMBLE";
const char kTypeDocumentSheetBackFlipped[] = "FLIPPED";

const struct RangeVendorCapabilityTypeNames {
  RangeVendorCapability::ValueType id;
  const char* json_name;
} kRangeVendorCapabilityTypeNames[] = {
    {RangeVendorCapability::ValueType::FLOAT, kTypeRangeVendorCapabilityFloat},
    {RangeVendorCapability::ValueType::INTEGER,
     kTypeRangeVendorCapabilityInteger},
};

const struct TypedValueVendorCapabilityTypeNames {
  TypedValueVendorCapability::ValueType id;
  const char* json_name;
} kTypedValueVendorCapabilityTypeNames[] = {
    {TypedValueVendorCapability::ValueType::BOOLEAN,
     kTypeTypedValueVendorCapabilityBoolean},
    {TypedValueVendorCapability::ValueType::FLOAT,
     kTypeTypedValueVendorCapabilityFloat},
    {TypedValueVendorCapability::ValueType::INTEGER,
     kTypeTypedValueVendorCapabilityInteger},
    {TypedValueVendorCapability::ValueType::STRING,
     kTypeTypedValueVendorCapabilityString},
};

const struct VendorCapabilityTypeNames {
  VendorCapability::Type id;
  const char* json_name;
} kVendorCapabilityTypeNames[] = {
    {VendorCapability::Type::RANGE, kTypeVendorCapabilityRange},
    {VendorCapability::Type::SELECT, kTypeVendorCapabilitySelect},
    {VendorCapability::Type::TYPED_VALUE, kTypeVendorCapabilityTypedValue},
};

const struct ColorNames {
  ColorType id;
  const char* const json_name;
} kColorNames[] = {
    {ColorType::STANDARD_COLOR, kTypeColorColor},
    {ColorType::STANDARD_MONOCHROME, kTypeColorMonochrome},
    {ColorType::CUSTOM_COLOR, kTypeColorCustomColor},
    {ColorType::CUSTOM_MONOCHROME, kTypeColorCustomMonochrome},
    {ColorType::AUTO_COLOR, kTypeColorAuto},
};

const struct DuplexNames {
  DuplexType id;
  const char* const json_name;
} kDuplexNames[] = {
    {DuplexType::NO_DUPLEX, kTypeDuplexNoDuplex},
    {DuplexType::LONG_EDGE, kTypeDuplexLongEdge},
    {DuplexType::SHORT_EDGE, kTypeDuplexShortEdge},
};

const struct OrientationNames {
  OrientationType id;
  const char* const json_name;
} kOrientationNames[] = {
    {OrientationType::PORTRAIT, kTypeOrientationPortrait},
    {OrientationType::LANDSCAPE, kTypeOrientationLandscape},
    {OrientationType::AUTO_ORIENTATION, kTypeOrientationAuto},
};

const struct MarginsNames {
  MarginsType id;
  const char* const json_name;
} kMarginsNames[] = {
    {MarginsType::NO_MARGINS, kTypeMarginsBorderless},
    {MarginsType::STANDARD_MARGINS, kTypeMarginsStandard},
    {MarginsType::CUSTOM_MARGINS, kTypeMarginsCustom},
};

const struct FitToPageNames {
  FitToPageType id;
  const char* const json_name;
} kFitToPageNames[] = {
    {FitToPageType::NO_FITTING, kTypeFitToPageNoFitting},
    {FitToPageType::FIT_TO_PAGE, kTypeFitToPageFitToPage},
    {FitToPageType::GROW_TO_PAGE, kTypeFitToPageGrowToPage},
    {FitToPageType::SHRINK_TO_PAGE, kTypeFitToPageShrinkToPage},
    {FitToPageType::FILL_PAGE, kTypeFitToPageFillPage},
};

const struct DocumentSheetBackNames {
  DocumentSheetBack id;
  const char* const json_name;
} kDocumentSheetBackNames[] = {
    {DocumentSheetBack::NORMAL, kTypeDocumentSheetBackNormal},
    {DocumentSheetBack::ROTATED, kTypeDocumentSheetBackRotated},
    {DocumentSheetBack::MANUAL_TUMBLE, kTypeDocumentSheetBackManualTumble},
    {DocumentSheetBack::FLIPPED, kTypeDocumentSheetBackFlipped}};

const int32_t kInchToUm = 25400;
const int32_t kMmToUm = 1000;
const int32_t kSizeTrasholdUm = 1000;

// Json name of media type is constructed by removing "MediaType::" enum class
// prefix from it.
#define MAP_CLOUD_PRINT_MEDIA_TYPE(type, width, height, unit_um) \
  {                                                              \
    type, &#type[strlen("MediaType::")],                         \
        static_cast<int>(width * unit_um + 0.5),                 \
        static_cast<int>(height * unit_um + 0.5)                 \
  }

const struct MediaDefinition {
  MediaType id;
  const char* const json_name;
  int width_um;
  int height_um;
} kMediaDefinitions[] = {
    {MediaType::CUSTOM_MEDIA, "CUSTOM", 0, 0},
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_INDEX_3X5, 3, 5, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_PERSONAL, 3.625f, 6.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_MONARCH, 3.875f, 7.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_NUMBER_9,
                               3.875f,
                               8.875f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_INDEX_4X6, 4, 6, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_NUMBER_10,
                               4.125f,
                               9.5f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_A2, 4.375f, 5.75f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_NUMBER_11,
                               4.5f,
                               10.375f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_NUMBER_12, 4.75f, 11, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_5X7, 5, 7, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_INDEX_5X8, 5, 8, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_NUMBER_14, 5, 11.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_INVOICE, 5.5f, 8.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_INDEX_4X6_EXT, 6, 8, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_6X9, 6, 9, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_C5, 6.5f, 9.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_7X9, 7, 9, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_EXECUTIVE,
                               7.25f,
                               10.5f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_GOVT_LETTER, 8, 10, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_GOVT_LEGAL, 8, 13, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_QUARTO, 8.5f, 10.83f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_LETTER, 8.5f, 11, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_FANFOLD_EUR, 8.5f, 12, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_LETTER_PLUS,
                               8.5f,
                               12.69f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_FOOLSCAP, 8.5f, 13, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_LEGAL, 8.5f, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_SUPER_A, 8.94f, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_9X11, 9, 11, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_ARCH_A, 9, 12, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_LETTER_EXTRA, 9.5f, 12, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_LEGAL_EXTRA, 9.5f, 15, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_10X11, 10, 11, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_10X13, 10, 13, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_10X14, 10, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_10X15, 10, 15, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_11X12, 11, 12, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_EDP, 11, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_FANFOLD_US,
                               11,
                               14.875f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_11X15, 11, 15, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_LEDGER, 11, 17, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_EUR_EDP, 12, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_ARCH_B, 12, 18, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_12X19, 12, 19, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_B_PLUS, 12, 19.17f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_SUPER_B, 13, 19, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_C, 17, 22, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_ARCH_C, 18, 24, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_D, 22, 34, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_ARCH_D, 24, 36, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_ASME_F, 28, 40, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_WIDE_FORMAT, 30, 42, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_E, 34, 44, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_ARCH_E, 36, 48, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::NA_F, 44, 68, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ROC_16K, 7.75f, 10.75f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ROC_8K, 10.75f, 15.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_32K, 97, 151, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_1, 102, 165, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_2, 102, 176, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_4, 110, 208, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_5, 110, 220, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_8, 120, 309, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_6, 120, 230, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_3, 125, 176, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_16K, 146, 215, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_7, 160, 230, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_JUURO_KU_KAI, 198, 275, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_PA_KAI, 267, 389, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_DAI_PA_KAI, 275, 395, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::PRC_10, 324, 458, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A10, 26, 37, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A9, 37, 52, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A8, 52, 74, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A7, 74, 105, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A6, 105, 148, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A5, 148, 210, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A5_EXTRA, 174, 235, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4, 210, 297, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4_TAB, 225, 297, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4_EXTRA, 235, 322, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A3, 297, 420, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4X3, 297, 630, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4X4, 297, 841, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4X5, 297, 1051, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4X6, 297, 1261, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4X7, 297, 1471, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4X8, 297, 1682, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A4X9, 297, 1892, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A3_EXTRA, 322, 445, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A2, 420, 594, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A3X3, 420, 891, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A3X4, 420, 1189, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A3X5, 420, 1486, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A3X6, 420, 1783, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A3X7, 420, 2080, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A1, 594, 841, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A2X3, 594, 1261, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A2X4, 594, 1682, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A2X5, 594, 2102, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A0, 841, 1189, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A1X3, 841, 1783, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A1X4, 841, 2378, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_2A0, 1189, 1682, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_A0X3, 1189, 2523, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B10, 31, 44, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B9, 44, 62, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B8, 62, 88, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B7, 88, 125, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B6, 125, 176, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B6C4, 125, 324, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B5, 176, 250, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B5_EXTRA, 201, 276, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B4, 250, 353, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B3, 353, 500, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B2, 500, 707, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B1, 707, 1000, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_B0, 1000, 1414, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C10, 28, 40, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C9, 40, 57, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C8, 57, 81, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C7, 81, 114, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C7C6, 81, 162, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C6, 114, 162, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C6C5, 114, 229, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C5, 162, 229, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C4, 229, 324, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C3, 324, 458, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C2, 458, 648, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C1, 648, 917, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_C0, 917, 1297, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_DL, 110, 220, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_RA2, 430, 610, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_SRA2, 450, 640, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_RA1, 610, 860, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_SRA1, 640, 900, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_RA0, 860, 1220, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::ISO_SRA0, 900, 1280, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B10, 32, 45, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B9, 45, 64, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B8, 64, 91, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B7, 91, 128, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B6, 128, 182, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B5, 182, 257, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B4, 257, 364, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B3, 364, 515, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B2, 515, 728, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B1, 728, 1030, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_B0, 1030, 1456, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JIS_EXEC, 216, 330, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JPN_CHOU4, 90, 205, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JPN_HAGAKI, 100, 148, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JPN_YOU4, 105, 235, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JPN_CHOU2, 111.1f, 146, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JPN_CHOU3, 120, 235, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JPN_OUFUKU, 148, 200, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JPN_KAHU, 240, 322.1f, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::JPN_KAKU2, 240, 332, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_SMALL_PHOTO, 100, 150, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_ITALIAN, 110, 230, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_POSTFIX, 114, 229, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_LARGE_PHOTO, 200, 300, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_FOLIO, 210, 330, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_FOLIO_SP, 215, 315, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_TYPE(MediaType::OM_INVITE, 220, 220, kMmToUm)};
#undef MAP_CLOUD_PRINT_MEDIA_TYPE

const MediaDefinition& FindMediaByType(MediaType type) {
  for (size_t i = 0; i < base::size(kMediaDefinitions); ++i) {
    if (kMediaDefinitions[i].id == type)
      return kMediaDefinitions[i];
  }
  NOTREACHED();
  return kMediaDefinitions[0];
}

const MediaDefinition* FindMediaBySize(int32_t width_um, int32_t height_um) {
  const MediaDefinition* result = nullptr;
  for (size_t i = 0; i < base::size(kMediaDefinitions); ++i) {
    int32_t diff =
        std::max(std::abs(width_um - kMediaDefinitions[i].width_um),
                 std::abs(height_um - kMediaDefinitions[i].height_um));
    if (diff < kSizeTrasholdUm)
      result = &kMediaDefinitions[i];
  }
  return result;
}

template <class T, class IdType>
std::string TypeToString(const T& names, IdType id) {
  for (size_t i = 0; i < base::size(names); ++i) {
    if (id == names[i].id)
      return names[i].json_name;
  }
  NOTREACHED();
  return std::string();
}

template <class T, class IdType>
bool TypeFromString(const T& names, const std::string& type, IdType* id) {
  for (size_t i = 0; i < base::size(names); ++i) {
    if (type == names[i].json_name) {
      *id = names[i].id;
      return true;
    }
  }
  return false;
}

}  // namespace

PwgRasterConfig::PwgRasterConfig()
    : document_sheet_back(DocumentSheetBack::ROTATED),
      reverse_order_streaming(false),
      rotate_all_pages(false) {}

PwgRasterConfig::~PwgRasterConfig() {}

RangeVendorCapability::RangeVendorCapability() = default;

RangeVendorCapability::RangeVendorCapability(ValueType value_type,
                                             const std::string& min_value,
                                             const std::string& max_value)
    : RangeVendorCapability(value_type,
                            min_value,
                            max_value,
                            /*default_value=*/"") {}

RangeVendorCapability::RangeVendorCapability(ValueType value_type,
                                             const std::string& min_value,
                                             const std::string& max_value,
                                             const std::string& default_value)
    : value_type_(value_type),
      min_value_(min_value),
      max_value_(max_value),
      default_value_(default_value) {}

RangeVendorCapability::RangeVendorCapability(RangeVendorCapability&& other) =
    default;

RangeVendorCapability::~RangeVendorCapability() = default;

RangeVendorCapability& RangeVendorCapability::operator=(
    RangeVendorCapability&& other) = default;

bool RangeVendorCapability::operator==(
    const RangeVendorCapability& other) const {
  return value_type_ == other.value_type_ && min_value_ == other.min_value_ &&
         max_value_ == other.max_value_ &&
         default_value_ == other.default_value_;
}

bool RangeVendorCapability::IsValid() const {
  if (min_value_.empty() || max_value_.empty())
    return false;
  switch (value_type_) {
    case ValueType::FLOAT: {
      double min_value;
      double max_value;
      if (!base::StringToDouble(min_value_, &min_value) ||
          !base::StringToDouble(max_value_, &max_value) ||
          min_value > max_value) {
        return false;
      }
      if (!default_value_.empty()) {
        double default_value;
        if (!base::StringToDouble(default_value_, &default_value) ||
            default_value < min_value || default_value > max_value) {
          return false;
        }
      }
      return true;
    }
    case ValueType::INTEGER: {
      int min_value;
      int max_value;
      if (!base::StringToInt(min_value_, &min_value) ||
          !base::StringToInt(max_value_, &max_value) || min_value > max_value) {
        return false;
      }
      if (!default_value_.empty()) {
        int default_value;
        if (!base::StringToInt(default_value_, &default_value) ||
            default_value < min_value || default_value > max_value) {
          return false;
        }
      }
      return true;
    }
  }
  NOTREACHED() << "Bad range capability value type";
  return false;
}

bool RangeVendorCapability::LoadFrom(const base::Value& dict) {
  const std::string* value_type_str = dict.FindStringKey(kKeyValueType);
  if (!value_type_str || !TypeFromString(kRangeVendorCapabilityTypeNames,
                                         *value_type_str, &value_type_)) {
    return false;
  }
  const std::string* min_value_str =
      dict.FindStringKey(kVendorCapabilityMinValue);
  if (!min_value_str)
    return false;
  min_value_ = *min_value_str;
  const std::string* max_value_str =
      dict.FindStringKey(kVendorCapabilityMaxValue);
  if (!max_value_str)
    return false;
  max_value_ = *max_value_str;
  const std::string* default_value_str =
      dict.FindStringKey(kVendorCapabilityDefaultValue);
  if (default_value_str)
    default_value_ = *default_value_str;
  return IsValid();
}

void RangeVendorCapability::SaveTo(base::Value* dict) const {
  DCHECK(IsValid());
  dict->SetStringKey(
      kKeyValueType,
      TypeToString(kRangeVendorCapabilityTypeNames, value_type_));
  dict->SetStringKey(kVendorCapabilityMinValue, min_value_);
  dict->SetStringKey(kVendorCapabilityMaxValue, max_value_);
  if (!default_value_.empty())
    dict->SetStringKey(kVendorCapabilityDefaultValue, default_value_);
}

SelectVendorCapabilityOption::SelectVendorCapabilityOption() = default;

SelectVendorCapabilityOption::SelectVendorCapabilityOption(
    const std::string& value,
    const std::string& display_name)
    : value(value), display_name(display_name) {}

SelectVendorCapabilityOption::~SelectVendorCapabilityOption() = default;

bool SelectVendorCapabilityOption::operator==(
    const SelectVendorCapabilityOption& other) const {
  return value == other.value && display_name == other.display_name;
}

bool SelectVendorCapabilityOption::IsValid() const {
  return !value.empty() && !display_name.empty();
}

TypedValueVendorCapability::TypedValueVendorCapability() = default;

TypedValueVendorCapability::TypedValueVendorCapability(ValueType value_type)
    : TypedValueVendorCapability(value_type, /*default_value=*/"") {}

TypedValueVendorCapability::TypedValueVendorCapability(
    ValueType value_type,
    const std::string& default_value)
    : value_type_(value_type), default_value_(default_value) {}

TypedValueVendorCapability::TypedValueVendorCapability(
    TypedValueVendorCapability&& other) = default;

TypedValueVendorCapability::~TypedValueVendorCapability() = default;

TypedValueVendorCapability& TypedValueVendorCapability::operator=(
    TypedValueVendorCapability&& other) = default;

bool TypedValueVendorCapability::operator==(
    const TypedValueVendorCapability& other) const {
  return value_type_ == other.value_type_ &&
         default_value_ == other.default_value_;
}

bool TypedValueVendorCapability::IsValid() const {
  if (default_value_.empty())
    return true;
  switch (value_type_) {
    case ValueType::BOOLEAN:
      return default_value_ == "true" || default_value_ == "false";
    case ValueType::FLOAT: {
      double value;
      return base::StringToDouble(default_value_, &value);
    }
    case ValueType::INTEGER: {
      int value;
      return base::StringToInt(default_value_, &value);
    }
    case ValueType::STRING:
      return true;
  }
  NOTREACHED() << "Bad typed value capability value type";
  return false;
}

bool TypedValueVendorCapability::LoadFrom(const base::Value& dict) {
  const std::string* value_type_str = dict.FindStringKey(kKeyValueType);
  if (!value_type_str || !TypeFromString(kTypedValueVendorCapabilityTypeNames,
                                         *value_type_str, &value_type_)) {
    return false;
  }
  const std::string* default_value_str =
      dict.FindStringKey(kVendorCapabilityDefaultValue);
  if (default_value_str)
    default_value_ = *default_value_str;
  return IsValid();
}

void TypedValueVendorCapability::SaveTo(base::Value* dict) const {
  DCHECK(IsValid());
  dict->SetStringKey(
      kKeyValueType,
      TypeToString(kTypedValueVendorCapabilityTypeNames, value_type_));
  if (!default_value_.empty())
    dict->SetStringKey(kVendorCapabilityDefaultValue, default_value_);
}

VendorCapability::VendorCapability() : type_(Type::NONE) {}

VendorCapability::VendorCapability(const std::string& id,
                                   const std::string& display_name,
                                   RangeVendorCapability range_capability)
    : type_(Type::RANGE),
      id_(id),
      display_name_(display_name),
      range_capability_(std::move(range_capability)) {}

VendorCapability::VendorCapability(const std::string& id,
                                   const std::string& display_name,
                                   SelectVendorCapability select_capability)
    : type_(Type::SELECT),
      id_(id),
      display_name_(display_name),
      select_capability_(std::move(select_capability)) {}

VendorCapability::VendorCapability(
    const std::string& id,
    const std::string& display_name,
    TypedValueVendorCapability typed_value_capability)
    : type_(Type::TYPED_VALUE),
      id_(id),
      display_name_(display_name),
      typed_value_capability_(std::move(typed_value_capability)) {}

VendorCapability::VendorCapability(VendorCapability&& other)
    : type_(other.type_), id_(other.id_), display_name_(other.display_name_) {
  switch (type_) {
    case Type::NONE:
      // No-op;
      break;
    case Type::RANGE:
      new (&range_capability_)
          RangeVendorCapability(std::move(other.range_capability_));
      break;
    case Type::SELECT:
      new (&select_capability_)
          SelectVendorCapability(std::move(other.select_capability_));
      break;
    case Type::TYPED_VALUE:
      new (&typed_value_capability_)
          TypedValueVendorCapability(std::move(other.typed_value_capability_));
      break;
    default:
      NOTREACHED();
  }
}

VendorCapability::~VendorCapability() {
  InternalCleanup();
}

void VendorCapability::InternalCleanup() {
  switch (type_) {
    case Type::NONE:
      break;
    case Type::RANGE:
      range_capability_.~RangeVendorCapability();
      break;
    case Type::SELECT:
      select_capability_.~SelectVendorCapability();
      break;
    case Type::TYPED_VALUE:
      typed_value_capability_.~TypedValueVendorCapability();
      break;
    default:
      NOTREACHED();
  }
  type_ = Type::NONE;
}

bool VendorCapability::operator==(const VendorCapability& other) const {
  if (type_ != other.type_ || id_ != other.id_ ||
      display_name_ != other.display_name_) {
    return false;
  }
  switch (type_) {
    case Type::NONE:
      return true;
    case Type::RANGE:
      return range_capability_ == other.range_capability_;
    case Type::SELECT:
      return select_capability_ == other.select_capability_;
    case Type::TYPED_VALUE:
      return typed_value_capability_ == other.typed_value_capability_;
  }
  NOTREACHED() << "Bad vendor capability type";
}

bool VendorCapability::IsValid() const {
  if (id_.empty() || display_name_.empty())
    return false;
  switch (type_) {
    case Type::NONE:
      return false;
    case Type::RANGE:
      return range_capability_.IsValid();
    case Type::SELECT:
      return select_capability_.IsValid();
    case Type::TYPED_VALUE:
      return typed_value_capability_.IsValid();
  }
  NOTREACHED() << "Bad vendor capability type";
  return false;
}

bool VendorCapability::LoadFrom(const base::Value& dict) {
  InternalCleanup();
  const std::string* type_str = dict.FindStringKey(kKeyType);
  Type type;
  if (!type_str ||
      !TypeFromString(kVendorCapabilityTypeNames, *type_str, &type)) {
    return false;
  }

  const std::string* id_str = dict.FindStringKey(kKeyId);
  if (!id_str)
    return false;

  id_ = *id_str;
  const std::string* display_name_str = dict.FindStringKey(kKeyDisplayName);
  if (!display_name_str)
    return false;

  display_name_ = *display_name_str;
  const base::Value* range_capability_value =
      dict.FindDictKey(kOptionRangeCapability);
  if (!range_capability_value == (type == Type::RANGE))
    return false;

  const base::Value* select_capability_value =
      dict.FindDictKey(kOptionSelectCapability);
  if (!select_capability_value == (type == Type::SELECT))
    return false;

  const base::Value* typed_value_capability_value =
      dict.FindDictKey(kOptionTypedValueCapability);
  if (!typed_value_capability_value == (type == Type::TYPED_VALUE))
    return false;

  type_ = type;
  switch (type_) {
    case Type::NONE:
    default:
      NOTREACHED();
      break;
    case Type::RANGE:
      new (&range_capability_) RangeVendorCapability();
      return range_capability_.LoadFrom(*range_capability_value);
    case Type::SELECT:
      new (&select_capability_) SelectVendorCapability();
      return select_capability_.LoadFrom(*select_capability_value);
    case Type::TYPED_VALUE:
      new (&typed_value_capability_) TypedValueVendorCapability();
      return typed_value_capability_.LoadFrom(*typed_value_capability_value);
  }

  return false;
}

void VendorCapability::SaveTo(base::Value* dict) const {
  DCHECK(IsValid());
  dict->SetStringKey(kKeyType, TypeToString(kVendorCapabilityTypeNames, type_));
  dict->SetStringKey(kKeyId, id_);
  dict->SetStringKey(kKeyDisplayName, display_name_);

  switch (type_) {
    case Type::NONE:
      NOTREACHED();
      break;
    case Type::RANGE: {
      base::Value range_capability_value(base::Value::Type::DICTIONARY);
      range_capability_.SaveTo(&range_capability_value);
      dict->SetKey(kOptionRangeCapability, std::move(range_capability_value));
      break;
    }
    case Type::SELECT: {
      base::Value select_capability_value(base::Value::Type::DICTIONARY);
      select_capability_.SaveTo(&select_capability_value);
      dict->SetKey(kOptionSelectCapability, std::move(select_capability_value));
      break;
    }
    case Type::TYPED_VALUE: {
      base::Value typed_value_capability_value(base::Value::Type::DICTIONARY);
      typed_value_capability_.SaveTo(&typed_value_capability_value);
      dict->SetKey(kOptionTypedValueCapability,
                   std::move(typed_value_capability_value));
      break;
    }
  }
}

Color::Color() : type(ColorType::AUTO_COLOR) {}

Color::Color(ColorType type) : type(type) {
}

bool Color::operator==(const Color& other) const {
  return type == other.type && vendor_id == other.vendor_id &&
         custom_display_name == other.custom_display_name;
}

bool Color::IsValid() const {
  if (type != ColorType::CUSTOM_COLOR && type != ColorType::CUSTOM_MONOCHROME)
    return true;
  return !vendor_id.empty() && !custom_display_name.empty();
}

Margins::Margins()
    : type(MarginsType::STANDARD_MARGINS),
      top_um(0),
      right_um(0),
      bottom_um(0),
      left_um(0) {}

Margins::Margins(MarginsType type,
                 int32_t top_um,
                 int32_t right_um,
                 int32_t bottom_um,
                 int32_t left_um)
    : type(type),
      top_um(top_um),
      right_um(right_um),
      bottom_um(bottom_um),
      left_um(left_um) {}

bool Margins::operator==(const Margins& other) const {
  return type == other.type && top_um == other.top_um &&
         right_um == other.right_um && bottom_um == other.bottom_um;
}

Dpi::Dpi() : horizontal(0), vertical(0) {
}

Dpi::Dpi(int32_t horizontal, int32_t vertical)
    : horizontal(horizontal), vertical(vertical) {}

bool Dpi::IsValid() const {
  return horizontal > 0 && vertical > 0;
}

bool Dpi::operator==(const Dpi& other) const {
  return horizontal == other.horizontal && vertical == other.vertical;
}

Media::Media()
    : type(MediaType::CUSTOM_MEDIA),
      width_um(0),
      height_um(0),
      is_continuous_feed(false) {}

Media::Media(MediaType type)
    : type(type),
      width_um(0),
      height_um(0),
      is_continuous_feed(false) {
  const MediaDefinition& media = FindMediaByType(type);
  width_um = media.width_um;
  height_um = media.height_um;
  is_continuous_feed = width_um <= 0 || height_um <= 0;
}

Media::Media(MediaType type, int32_t width_um, int32_t height_um)
    : type(type),
      width_um(width_um),
      height_um(height_um),
      is_continuous_feed(width_um <= 0 || height_um <= 0) {}

Media::Media(const std::string& custom_display_name,
             const std::string& vendor_id,
             int32_t width_um,
             int32_t height_um)
    : type(MediaType::CUSTOM_MEDIA),
      width_um(width_um),
      height_um(height_um),
      is_continuous_feed(width_um <= 0 || height_um <= 0),
      custom_display_name(custom_display_name),
      vendor_id(vendor_id) {}

Media::Media(const Media& other) = default;

bool Media::MatchBySize() {
  const MediaDefinition* media = FindMediaBySize(width_um, height_um);
  if (!media)
    return false;
  type = media->id;
  return true;
}

bool Media::IsValid() const {
  if (is_continuous_feed) {
    if (width_um <= 0 && height_um <= 0)
      return false;
  } else {
    if (width_um <= 0 || height_um <= 0)
      return false;
  }
  return true;
}

bool Media::operator==(const Media& other) const {
  return type == other.type && width_um == other.width_um &&
         height_um == other.height_um &&
         is_continuous_feed == other.is_continuous_feed;
}

Interval::Interval() : start(0), end(0) {
}

Interval::Interval(int32_t start, int32_t end) : start(start), end(end) {}

Interval::Interval(int32_t start) : start(start), end(kMaxPageNumber) {}

bool Interval::operator==(const Interval& other) const {
  return start == other.start && end == other.end;
}

template <const char* kName>
class ItemsTraits {
 public:
  static std::vector<base::StringPiece> GetCapabilityPath() {
    return {kSectionPrinter, kName};
  }

  static std::vector<base::StringPiece> GetTicketItemPath() {
    return {kSectionPrint, kName};
  }
};

class NoValueValidation {
 public:
  template <class Option>
  static bool IsValid(const Option&) {
    return true;
  }
};

class ContentTypeTraits : public NoValueValidation,
                          public ItemsTraits<kOptionContentType> {
 public:
  static bool Load(const base::Value& dict, ContentType* option) {
    const std::string* content_type = dict.FindStringKey(kKeyContentType);
    if (!content_type)
      return false;
    *option = *content_type;
    return true;
  }

  static void Save(ContentType option, base::Value* dict) {
    dict->SetKey(kKeyContentType, base::Value(option));
  }
};

class PwgRasterConfigTraits : public NoValueValidation,
                              public ItemsTraits<kOptionPwgRasterConfig> {
 public:
  static bool Load(const base::Value& dict, PwgRasterConfig* option) {
    PwgRasterConfig option_out;
    const base::Value* document_sheet_back =
        dict.FindKey(kPwgRasterDocumentSheetBack);
    if (document_sheet_back) {
      if (!document_sheet_back->is_string() ||
          !TypeFromString(kDocumentSheetBackNames,
                          document_sheet_back->GetString(),
                          &option_out.document_sheet_back)) {
        return false;
      }
    }

    const base::Value* document_types_supported =
        dict.FindKey(kPwgRasterDocumentTypeSupported);
    if (document_types_supported) {
      if (!document_types_supported->is_list())
        return false;
      for (const auto& type_value : document_types_supported->GetList()) {
        if (!type_value.is_string())
          return false;

        const std::string& type = type_value.GetString();
        if (type == kTypeDocumentSupportedTypeSRGB8) {
          option_out.document_types_supported.push_back(
              PwgDocumentTypeSupported::SRGB_8);
        } else if (type == kTypeDocumentSupportedTypeSGRAY8) {
          option_out.document_types_supported.push_back(
              PwgDocumentTypeSupported::SGRAY_8);
        }
      }
    }

    option_out.reverse_order_streaming =
        dict.FindBoolKey(kPwgRasterReverseOrderStreaming).value_or(false);
    option_out.rotate_all_pages =
        dict.FindBoolKey(kPwgRasterRotateAllPages).value_or(false);
    *option = option_out;
    return true;
  }

  static void Save(const PwgRasterConfig& option, base::Value* dict) {
    dict->SetKey(kPwgRasterDocumentSheetBack,
                 base::Value(TypeToString(kDocumentSheetBackNames,
                                          option.document_sheet_back)));

    if (!option.document_types_supported.empty()) {
      base::Value::ListStorage supported_list;
      for (const auto& type : option.document_types_supported) {
        switch (type) {
          case PwgDocumentTypeSupported::SRGB_8:
            supported_list.push_back(
                base::Value(kTypeDocumentSupportedTypeSRGB8));
            break;
          case PwgDocumentTypeSupported::SGRAY_8:
            supported_list.push_back(
                base::Value(kTypeDocumentSupportedTypeSGRAY8));
            break;
        }
      }
      dict->SetKey(kPwgRasterDocumentTypeSupported,
                   base::Value(supported_list));
    }

    if (option.reverse_order_streaming) {
      dict->SetKey(kPwgRasterReverseOrderStreaming,
                   base::Value(option.reverse_order_streaming));
    }

    if (option.rotate_all_pages) {
      dict->SetKey(kPwgRasterRotateAllPages,
                   base::Value(option.rotate_all_pages));
    }
  }
};

class VendorCapabilityTraits : public ItemsTraits<kOptionVendorCapability> {
 public:
  static bool IsValid(const VendorCapability& option) {
    return option.IsValid();
  }

  static bool Load(const base::Value& dict, VendorCapability* option) {
    return option->LoadFrom(dict);
  }

  static void Save(const VendorCapability& option, base::Value* dict) {
    option.SaveTo(dict);
  }
};

class SelectVendorCapabilityTraits
    : public ItemsTraits<kOptionSelectCapability> {
 public:
  static bool IsValid(const SelectVendorCapabilityOption& option) {
    return option.IsValid();
  }

  static bool Load(const base::Value& dict,
                   SelectVendorCapabilityOption* option) {
    const std::string* value = dict.FindStringKey(kKeyValue);
    if (!value)
      return false;
    option->value = *value;
    const std::string* display_name = dict.FindStringKey(kKeyDisplayName);
    if (!display_name)
      return false;
    option->display_name = *display_name;
    return true;
  }

  static void Save(const SelectVendorCapabilityOption& option,
                   base::Value* dict) {
    dict->SetKey(kKeyValue, base::Value(option.value));
    dict->SetKey(kKeyDisplayName, base::Value(option.display_name));
  }
};

class ColorTraits : public ItemsTraits<kOptionColor> {
 public:
  static bool IsValid(const Color& option) { return option.IsValid(); }

  static bool Load(const base::Value& dict, Color* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    if (!type || !TypeFromString(kColorNames, *type, &option->type))
      return false;
    const std::string* vendor_id = dict.FindStringKey(kKeyVendorId);
    if (vendor_id)
      option->vendor_id = *vendor_id;
    const std::string* custom_display_name =
        dict.FindStringKey(kKeyCustomDisplayName);
    if (custom_display_name)
      option->custom_display_name = *custom_display_name;
    return true;
  }

  static void Save(const Color& option, base::Value* dict) {
    dict->SetKey(kKeyType, base::Value(TypeToString(kColorNames, option.type)));
    if (!option.vendor_id.empty())
      dict->SetKey(kKeyVendorId, base::Value(option.vendor_id));
    if (!option.custom_display_name.empty()) {
      dict->SetKey(kKeyCustomDisplayName,
                   base::Value(option.custom_display_name));
    }
  }
};

class DuplexTraits : public NoValueValidation,
                     public ItemsTraits<kOptionDuplex> {
 public:
  static bool Load(const base::Value& dict, DuplexType* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    return type && TypeFromString(kDuplexNames, *type, option);
  }

  static void Save(DuplexType option, base::Value* dict) {
    dict->SetKey(kKeyType, base::Value(TypeToString(kDuplexNames, option)));
  }
};

class OrientationTraits : public NoValueValidation,
                          public ItemsTraits<kOptionPageOrientation> {
 public:
  static bool Load(const base::Value& dict, OrientationType* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    return type && TypeFromString(kOrientationNames, *type, option);
  }

  static void Save(OrientationType option, base::Value* dict) {
    dict->SetKey(kKeyType,
                 base::Value(TypeToString(kOrientationNames, option)));
  }
};

class CopiesTraits : public ItemsTraits<kOptionCopies> {
 public:
  static bool IsValid(int32_t option) { return option >= 1; }

  static bool Load(const base::Value& dict, int32_t* option) {
    base::Optional<int> copies = dict.FindIntKey(kOptionCopies);
    if (!copies)
      return false;
    *option = copies.value();
    return true;
  }

  static void Save(int32_t option, base::Value* dict) {
    dict->SetKey(kOptionCopies, base::Value(option));
  }
};

class MarginsTraits : public NoValueValidation,
                      public ItemsTraits<kOptionMargins> {
 public:
  static bool Load(const base::Value& dict, Margins* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    if (!type || !TypeFromString(kMarginsNames, *type, &option->type))
      return false;
    base::Optional<int> top_um = dict.FindIntKey(kMarginTop);
    base::Optional<int> right_um = dict.FindIntKey(kMarginRight);
    base::Optional<int> bottom_um = dict.FindIntKey(kMarginBottom);
    base::Optional<int> left_um = dict.FindIntKey(kMarginLeft);
    if (!top_um || !right_um || !bottom_um || !left_um)
      return false;
    option->top_um = top_um.value();
    option->right_um = right_um.value();
    option->bottom_um = bottom_um.value();
    option->left_um = left_um.value();
    return true;
  }

  static void Save(const Margins& option, base::Value* dict) {
    dict->SetKey(kKeyType,
                 base::Value(TypeToString(kMarginsNames, option.type)));
    dict->SetKey(kMarginTop, base::Value(option.top_um));
    dict->SetKey(kMarginRight, base::Value(option.right_um));
    dict->SetKey(kMarginBottom, base::Value(option.bottom_um));
    dict->SetKey(kMarginLeft, base::Value(option.left_um));
  }
};

class DpiTraits : public ItemsTraits<kOptionDpi> {
 public:
  static bool IsValid(const Dpi& option) { return option.IsValid(); }

  static bool Load(const base::Value& dict, Dpi* option) {
    base::Optional<int> horizontal = dict.FindIntKey(kDpiHorizontal);
    base::Optional<int> vertical = dict.FindIntKey(kDpiVertical);
    if (!horizontal || !vertical)
      return false;
    option->horizontal = horizontal.value();
    option->vertical = vertical.value();
    return true;
  }

  static void Save(const Dpi& option, base::Value* dict) {
    dict->SetKey(kDpiHorizontal, base::Value(option.horizontal));
    dict->SetKey(kDpiVertical, base::Value(option.vertical));
  }
};

class FitToPageTraits : public NoValueValidation,
                        public ItemsTraits<kOptionFitToPage> {
 public:
  static bool Load(const base::Value& dict, FitToPageType* option) {
    const std::string* type = dict.FindStringKey(kKeyType);
    return type && TypeFromString(kFitToPageNames, *type, option);
  }

  static void Save(FitToPageType option, base::Value* dict) {
    dict->SetKey(kKeyType, base::Value(TypeToString(kFitToPageNames, option)));
  }
};

class PageRangeTraits : public ItemsTraits<kOptionPageRange> {
 public:
  static bool IsValid(const PageRange& option) {
    for (size_t i = 0; i < option.size(); ++i) {
      if (option[i].start < 1 || option[i].end < 1) {
        return false;
      }
    }
    return true;
  }

  static bool Load(const base::Value& dict, PageRange* option) {
    const base::Value* list_value =
        dict.FindKeyOfType(kPageRangeInterval, base::Value::Type::LIST);
    if (!list_value)
      return false;
    base::span<const base::Value> list = list_value->GetList();
    for (const base::Value& interval : list) {
      int page_range_start = interval.FindIntKey(kPageRangeStart).value_or(1);
      int page_range_end =
          interval.FindIntKey(kPageRangeEnd).value_or(kMaxPageNumber);
      option->push_back(Interval(page_range_start, page_range_end));
    }
    return true;
  }

  static void Save(const PageRange& option, base::Value* dict) {
    if (!option.empty()) {
      base::Value list(base::Value::Type::LIST);
      for (size_t i = 0; i < option.size(); ++i) {
        base::Value interval(base::Value::Type::DICTIONARY);
        interval.SetKey(kPageRangeStart, base::Value(option[i].start));
        if (option[i].end < kMaxPageNumber)
          interval.SetKey(kPageRangeEnd, base::Value(option[i].end));
        list.Append(std::move(interval));
      }
      dict->SetKey(kPageRangeInterval, std::move(list));
    }
  }
};

class MediaTraits : public ItemsTraits<kOptionMediaSize> {
 public:
  static bool IsValid(const Media& option) { return option.IsValid(); }

  static bool Load(const base::Value& dict, Media* option) {
    const std::string* type = dict.FindStringKey(kKeyName);
    if (type && !TypeFromString(kMediaDefinitions, *type, &option->type))
      return false;
    base::Optional<int> width_um = dict.FindIntKey(kMediaWidth);
    if (width_um)
      option->width_um = width_um.value();
    base::Optional<int> height_um = dict.FindIntKey(kMediaHeight);
    if (height_um)
      option->height_um = height_um.value();
    base::Optional<bool> is_continuous_feed =
        dict.FindBoolKey(kMediaIsContinuous);
    if (is_continuous_feed)
      option->is_continuous_feed = is_continuous_feed.value();
    const std::string* custom_display_name =
        dict.FindStringKey(kKeyCustomDisplayName);
    if (custom_display_name)
      option->custom_display_name = *custom_display_name;
    const std::string* vendor_id = dict.FindStringKey(kKeyVendorId);
    if (vendor_id)
      option->vendor_id = *vendor_id;
    return true;
  }

  static void Save(const Media& option, base::Value* dict) {
    if (option.type != MediaType::CUSTOM_MEDIA) {
      dict->SetKey(kKeyName,
                   base::Value(TypeToString(kMediaDefinitions, option.type)));
    }
    if (!option.custom_display_name.empty() ||
        option.type == MediaType::CUSTOM_MEDIA) {
      dict->SetKey(kKeyCustomDisplayName,
                   base::Value(option.custom_display_name));
    }
    if (!option.vendor_id.empty())
      dict->SetKey(kKeyVendorId, base::Value(option.vendor_id));
    if (option.width_um > 0)
      dict->SetKey(kMediaWidth, base::Value(option.width_um));
    if (option.height_um > 0)
      dict->SetKey(kMediaHeight, base::Value(option.height_um));
    if (option.is_continuous_feed)
      dict->SetKey(kMediaIsContinuous, base::Value(true));
  }
};

class CollateTraits : public NoValueValidation,
                      public ItemsTraits<kOptionCollate> {
 public:
  static const bool kDefault = true;

  static bool Load(const base::Value& dict, bool* option) {
    base::Optional<bool> collate = dict.FindBoolKey(kOptionCollate);
    if (!collate)
      return false;
    *option = collate.value();
    return true;
  }

  static void Save(bool option, base::Value* dict) {
    dict->SetKey(kOptionCollate, base::Value(option));
  }
};

class ReverseTraits : public NoValueValidation,
                      public ItemsTraits<kOptionReverse> {
 public:
  static const bool kDefault = false;

  static bool Load(const base::Value& dict, bool* option) {
    base::Optional<bool> reverse = dict.FindBoolKey(kOptionReverse);
    if (!reverse)
      return false;
    *option = reverse.value();
    return true;
  }

  static void Save(bool option, base::Value* dict) {
    dict->SetKey(kOptionReverse, base::Value(option));
  }
};

#if defined(OS_CHROMEOS)
class PinTraits : public NoValueValidation, public ItemsTraits<kOptionPin> {
 public:
  static bool Load(const base::Value& dict, bool* option) {
    base::Optional<bool> supported = dict.FindBoolKey(kPinSupported);
    if (!supported)
      return false;
    *option = supported.value();
    return true;
  }

  static void Save(bool option, base::Value* dict) {
    dict->SetKey(kPinSupported, base::Value(option));
  }
};
#endif  // defined(OS_CHROMEOS)

}  // namespace printer

template class ListCapability<printer::ContentType, printer::ContentTypeTraits>;
template class ValueCapability<printer::PwgRasterConfig,
                               printer::PwgRasterConfigTraits>;
template class ListCapability<printer::VendorCapability,
                              printer::VendorCapabilityTraits>;
template class SelectionCapability<printer::SelectVendorCapabilityOption,
                                   printer::SelectVendorCapabilityTraits>;
template class SelectionCapability<printer::Color, printer::ColorTraits>;
template class SelectionCapability<printer::DuplexType, printer::DuplexTraits>;
template class SelectionCapability<printer::OrientationType,
                                   printer::OrientationTraits>;
template class SelectionCapability<printer::Margins, printer::MarginsTraits>;
template class SelectionCapability<printer::Dpi, printer::DpiTraits>;
template class SelectionCapability<printer::FitToPageType,
                                   printer::FitToPageTraits>;
template class SelectionCapability<printer::Media, printer::MediaTraits>;
template class EmptyCapability<printer::CopiesTraits>;
template class EmptyCapability<printer::PageRangeTraits>;
template class BooleanCapability<printer::CollateTraits>;
template class BooleanCapability<printer::ReverseTraits>;
#if defined(OS_CHROMEOS)
template class ValueCapability<bool, printer::PinTraits>;
#endif  // defined(OS_CHROMEOS)

template class TicketItem<printer::PwgRasterConfig,
                          printer::PwgRasterConfigTraits>;
template class TicketItem<printer::Color, printer::ColorTraits>;
template class TicketItem<printer::DuplexType, printer::DuplexTraits>;
template class TicketItem<printer::OrientationType, printer::OrientationTraits>;
template class TicketItem<printer::Margins, printer::MarginsTraits>;
template class TicketItem<printer::Dpi, printer::DpiTraits>;
template class TicketItem<printer::FitToPageType, printer::FitToPageTraits>;
template class TicketItem<printer::Media, printer::MediaTraits>;
template class TicketItem<int32_t, printer::CopiesTraits>;
template class TicketItem<printer::PageRange, printer::PageRangeTraits>;
template class TicketItem<bool, printer::CollateTraits>;
template class TicketItem<bool, printer::ReverseTraits>;

}  // namespace cloud_devices
