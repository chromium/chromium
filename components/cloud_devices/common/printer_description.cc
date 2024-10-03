// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/printer_description.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/cloud_devices/common/cloud_device_description_consts.h"
#include "components/cloud_devices/common/description_items_inl.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cloud_devices {

namespace printer {

namespace {

constexpr int32_t kMaxPageNumber = 1000000;

constexpr char kSectionPrint[] = "print";
constexpr char kSectionPrinter[] = "printer";

constexpr char kKeyCustomDisplayName[] = "custom_display_name";
constexpr char kKeyContentType[] = "content_type";
constexpr char kKeyDisplayName[] = "display_name";
constexpr char kKeyId[] = "id";
constexpr char kKeyName[] = "name";
constexpr char kKeyType[] = "type";
constexpr char kKeyValue[] = "value";
constexpr char kKeyValueType[] = "value_type";
constexpr char kKeyVendorId[] = "vendor_id";

// extern is required to be used in templates.
extern constexpr char kOptionCollate[] = "collate";
extern constexpr char kOptionColor[] = "color";
extern constexpr char kOptionContentType[] = "supported_content_type";
extern constexpr char kOptionCopies[] = "copies";
extern constexpr char kOptionDpi[] = "dpi";
extern constexpr char kOptionDuplex[] = "duplex";
extern constexpr char kOptionFitToPage[] = "fit_to_page";
extern constexpr char kOptionMargins[] = "margins";
extern constexpr char kOptionMediaSize[] = "media_size";
extern constexpr char kOptionMediaType[] = "media_type";
extern constexpr char kOptionPageOrientation[] = "page_orientation";
extern constexpr char kOptionPageRange[] = "page_range";
extern constexpr char kOptionReverse[] = "reverse_order";
extern constexpr char kOptionPwgRasterConfig[] = "pwg_raster_config";
extern constexpr char kOptionRangeCapability[] = "range_cap";
extern constexpr char kOptionSelectCapability[] = "select_cap";
extern constexpr char kOptionTypedValueCapability[] = "typed_value_cap";
extern constexpr char kOptionVendorCapability[] = "vendor_capability";
extern constexpr char kOptionVendorItem[] = "vendor_ticket_item";
#if BUILDFLAG(IS_CHROMEOS)
extern constexpr char kOptionPin[] = "pin";
#endif  // BUILDFLAG(IS_CHROMEOS)

constexpr char kMarginBottom[] = "bottom_microns";
constexpr char kMarginLeft[] = "left_microns";
constexpr char kMarginRight[] = "right_microns";
constexpr char kMarginTop[] = "top_microns";

constexpr char kDpiHorizontal[] = "horizontal_dpi";
constexpr char kDpiVertical[] = "vertical_dpi";

constexpr char kMediaWidth[] = "width_microns";
constexpr char kMediaHeight[] = "height_microns";
constexpr char kMediaIsContinuous[] = "is_continuous_feed";
constexpr char kMediaImageableAreaLeft[] = "imageable_area_left_microns";
constexpr char kMediaImageableAreaBottom[] = "imageable_area_bottom_microns";
constexpr char kMediaImageableAreaRight[] = "imageable_area_right_microns";
constexpr char kMediaImageableAreaTop[] = "imageable_area_top_microns";
constexpr char kMediaMinHeight[] = "min_height_microns";
constexpr char kMediaMaxHeight[] = "max_height_microns";
constexpr char kMediaHasBorderlessVariant[] = "has_borderless_variant";

constexpr char kPageRangeInterval[] = "interval";
constexpr char kPageRangeEnd[] = "end";
constexpr char kPageRangeStart[] = "start";

constexpr char kPwgRasterDocumentTypeSupported[] = "document_type_supported";
constexpr char kPwgRasterDocumentSheetBack[] = "document_sheet_back";
constexpr char kPwgRasterReverseOrderStreaming[] = "reverse_order_streaming";
constexpr char kPwgRasterRotateAllPages[] = "rotate_all_pages";

constexpr char kMinValue[] = "min";
constexpr char kMaxValue[] = "max";
constexpr char kDefaultValue[] = "default";

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kPinSupported[] = "supported";
#endif  // BUILDFLAG(IS_CHROMEOS)

constexpr char kTypeRangeVendorCapabilityFloat[] = "FLOAT";
constexpr char kTypeRangeVendorCapabilityInteger[] = "INTEGER";

constexpr char kTypeTypedValueVendorCapabilityBoolean[] = "BOOLEAN";
constexpr char kTypeTypedValueVendorCapabilityFloat[] = "FLOAT";
constexpr char kTypeTypedValueVendorCapabilityInteger[] = "INTEGER";
constexpr char kTypeTypedValueVendorCapabilityString[] = "STRING";

constexpr char kTypeVendorCapabilityRange[] = "RANGE";
constexpr char kTypeVendorCapabilitySelect[] = "SELECT";
constexpr char kTypeVendorCapabilityTypedValue[] = "TYPED_VALUE";

constexpr char kTypeColorColor[] = "STANDARD_COLOR";
constexpr char kTypeColorMonochrome[] = "STANDARD_MONOCHROME";
constexpr char kTypeColorCustomColor[] = "CUSTOM_COLOR";
constexpr char kTypeColorCustomMonochrome[] = "CUSTOM_MONOCHROME";
constexpr char kTypeColorAuto[] = "AUTO";

constexpr char kTypeDuplexLongEdge[] = "LONG_EDGE";
constexpr char kTypeDuplexNoDuplex[] = "NO_DUPLEX";
constexpr char kTypeDuplexShortEdge[] = "SHORT_EDGE";

constexpr char kTypeFitToPageFillPage[] = "FILL_PAGE";
constexpr char kTypeFitToPageFitToPage[] = "FIT_TO_PAGE";
constexpr char kTypeFitToPageGrowToPage[] = "GROW_TO_PAGE";
constexpr char kTypeFitToPageNoFitting[] = "NO_FITTING";
constexpr char kTypeFitToPageShrinkToPage[] = "SHRINK_TO_PAGE";

constexpr char kTypeMarginsBorderless[] = "BORDERLESS";
constexpr char kTypeMarginsCustom[] = "CUSTOM";
constexpr char kTypeMarginsStandard[] = "STANDARD";
constexpr char kTypeOrientationAuto[] = "AUTO";

constexpr char kTypeOrientationLandscape[] = "LANDSCAPE";
constexpr char kTypeOrientationPortrait[] = "PORTRAIT";

constexpr char kTypeDocumentSupportedTypeSRGB8[] = "SRGB_8";
constexpr char kTypeDocumentSupportedTypeSGRAY8[] = "SGRAY_8";

constexpr char kTypeDocumentSheetBackNormal[] = "NORMAL";
constexpr char kTypeDocumentSheetBackRotated[] = "ROTATED";
constexpr char kTypeDocumentSheetBackManualTumble[] = "MANUAL_TUMBLE";
constexpr char kTypeDocumentSheetBackFlipped[] = "FLIPPED";

constexpr struct RangeVendorCapabilityTypeNames {
  RangeVendorCapability::ValueType id;
  const char* json_name;
} kRangeVendorCapabilityTypeNames[] = {
    {RangeVendorCapability::ValueType::FLOAT, kTypeRangeVendorCapabilityFloat},
    {RangeVendorCapability::ValueType::INTEGER,
     kTypeRangeVendorCapabilityInteger},
};

constexpr struct TypedValueVendorCapabilityTypeNames {
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

constexpr struct VendorCapabilityTypeNames {
  VendorCapability::Type id;
  const char* json_name;
} kVendorCapabilityTypeNames[] = {
    {VendorCapability::Type::RANGE, kTypeVendorCapabilityRange},
    {VendorCapability::Type::SELECT, kTypeVendorCapabilitySelect},
    {VendorCapability::Type::TYPED_VALUE, kTypeVendorCapabilityTypedValue},
};

constexpr struct ColorNames {
  ColorType id;
  const char* const json_name;
} kColorNames[] = {
    {ColorType::STANDARD_COLOR, kTypeColorColor},
    {ColorType::STANDARD_MONOCHROME, kTypeColorMonochrome},
    {ColorType::CUSTOM_COLOR, kTypeColorCustomColor},
    {ColorType::CUSTOM_MONOCHROME, kTypeColorCustomMonochrome},
    {ColorType::AUTO_COLOR, kTypeColorAuto},
};

constexpr struct DuplexNames {
  DuplexType id;
  const char* const json_name;
} kDuplexNames[] = {
    {DuplexType::NO_DUPLEX, kTypeDuplexNoDuplex},
    {DuplexType::LONG_EDGE, kTypeDuplexLongEdge},
    {DuplexType::SHORT_EDGE, kTypeDuplexShortEdge},
};

constexpr struct OrientationNames {
  OrientationType id;
  const char* const json_name;
} kOrientationNames[] = {
    {OrientationType::PORTRAIT, kTypeOrientationPortrait},
    {OrientationType::LANDSCAPE, kTypeOrientationLandscape},
    {OrientationType::AUTO_ORIENTATION, kTypeOrientationAuto},
};

constexpr struct MarginsNames {
  MarginsType id;
  const char* const json_name;
} kMarginsNames[] = {
    {MarginsType::NO_MARGINS, kTypeMarginsBorderless},
    {MarginsType::STANDARD_MARGINS, kTypeMarginsStandard},
    {MarginsType::CUSTOM_MARGINS, kTypeMarginsCustom},
};

constexpr struct FitToPageNames {
  FitToPageType id;
  const char* const json_name;
} kFitToPageNames[] = {
    {FitToPageType::NO_FITTING, kTypeFitToPageNoFitting},
    {FitToPageType::FIT_TO_PAGE, kTypeFitToPageFitToPage},
    {FitToPageType::GROW_TO_PAGE, kTypeFitToPageGrowToPage},
    {FitToPageType::SHRINK_TO_PAGE, kTypeFitToPageShrinkToPage},
    {FitToPageType::FILL_PAGE, kTypeFitToPageFillPage},
};

constexpr struct DocumentSheetBackNames {
  DocumentSheetBack id;
  const char* const json_name;
} kDocumentSheetBackNames[] = {
    {DocumentSheetBack::NORMAL, kTypeDocumentSheetBackNormal},
    {DocumentSheetBack::ROTATED, kTypeDocumentSheetBackRotated},
    {DocumentSheetBack::MANUAL_TUMBLE, kTypeDocumentSheetBackManualTumble},
    {DocumentSheetBack::FLIPPED, kTypeDocumentSheetBackFlipped}};

constexpr int32_t kInchToUm = 25400;
constexpr int32_t kMmToUm = 1000;
constexpr int32_t kSizeThresholdUm = 1000;

constexpr size_t kEnumClassPrefixLen = std::size("MediaSize::") - 1;

// Json name of media type is constructed by removing "MediaSize::" enum class
// prefix from it.
#define MAP_CLOUD_PRINT_MEDIA_SIZE(type, width, height, unit_um) \
  {                                                              \
    type, &#type[kEnumClassPrefixLen],                           \
        gfx::Size(static_cast<int>(width * unit_um + 0.5),       \
                  static_cast<int>(height * unit_um + 0.5))      \
  }

constexpr struct MediaDefinition {
  MediaSize id;
  const char* const json_name;
  gfx::Size size_um;
} kMediaDefinitions[] = {
    {MediaSize::CUSTOM_MEDIA, "CUSTOM", gfx::Size()},
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_INDEX_3X5, 3, 5, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_PERSONAL, 3.625f, 6.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_MONARCH, 3.875f, 7.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_NUMBER_9,
                               3.875f,
                               8.875f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_INDEX_4X6, 4, 6, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_NUMBER_10,
                               4.125f,
                               9.5f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_A2, 4.375f, 5.75f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_NUMBER_11,
                               4.5f,
                               10.375f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_NUMBER_12, 4.75f, 11, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_5X7, 5, 7, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_INDEX_5X8, 5, 8, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_NUMBER_14, 5, 11.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_INVOICE, 5.5f, 8.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_INDEX_4X6_EXT, 6, 8, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_6X9, 6, 9, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_C5, 6.5f, 9.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_7X9, 7, 9, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_EXECUTIVE,
                               7.25f,
                               10.5f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_GOVT_LETTER, 8, 10, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_GOVT_LEGAL, 8, 13, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_QUARTO, 8.5f, 10.83f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_LETTER, 8.5f, 11, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_FANFOLD_EUR, 8.5f, 12, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_LETTER_PLUS,
                               8.5f,
                               12.69f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_FOOLSCAP, 8.5f, 13, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_LEGAL, 8.5f, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_SUPER_A, 8.94f, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_9X11, 9, 11, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_ARCH_A, 9, 12, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_LETTER_EXTRA, 9.5f, 12, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_LEGAL_EXTRA, 9.5f, 15, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_10X11, 10, 11, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_10X13, 10, 13, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_10X14, 10, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_10X15, 10, 15, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_11X12, 11, 12, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_EDP, 11, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_FANFOLD_US,
                               11,
                               14.875f,
                               kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_11X15, 11, 15, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_LEDGER, 11, 17, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_EUR_EDP, 12, 14, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_ARCH_B, 12, 18, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_12X19, 12, 19, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_B_PLUS, 12, 19.17f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_SUPER_B, 13, 19, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_C, 17, 22, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_ARCH_C, 18, 24, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_D, 22, 34, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_ARCH_D, 24, 36, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_ASME_F, 28, 40, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_WIDE_FORMAT, 30, 42, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_E, 34, 44, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_ARCH_E, 36, 48, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::NA_F, 44, 68, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ROC_16K, 7.75f, 10.75f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ROC_8K, 10.75f, 15.5f, kInchToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_32K, 97, 151, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_1, 102, 165, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_2, 102, 176, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_4, 110, 208, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_5, 110, 220, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_8, 120, 309, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_6, 120, 230, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_3, 125, 176, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_16K, 146, 215, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_7, 160, 230, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_JUURO_KU_KAI, 198, 275, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_PA_KAI, 267, 389, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_DAI_PA_KAI, 275, 395, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::PRC_10, 324, 458, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A10, 26, 37, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A9, 37, 52, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A8, 52, 74, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A7, 74, 105, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A6, 105, 148, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A5, 148, 210, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A5_EXTRA, 174, 235, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4, 210, 297, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4_TAB, 225, 297, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4_EXTRA, 235, 322, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A3, 297, 420, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4X3, 297, 630, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4X4, 297, 841, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4X5, 297, 1051, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4X6, 297, 1261, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4X7, 297, 1471, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4X8, 297, 1682, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A4X9, 297, 1892, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A3_EXTRA, 322, 445, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A2, 420, 594, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A3X3, 420, 891, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A3X4, 420, 1189, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A3X5, 420, 1486, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A3X6, 420, 1783, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A3X7, 420, 2080, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A1, 594, 841, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A2X3, 594, 1261, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A2X4, 594, 1682, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A2X5, 594, 2102, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A0, 841, 1189, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A1X3, 841, 1783, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A1X4, 841, 2378, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_2A0, 1189, 1682, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_A0X3, 1189, 2523, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B10, 31, 44, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B9, 44, 62, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B8, 62, 88, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B7, 88, 125, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B6, 125, 176, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B6C4, 125, 324, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B5, 176, 250, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B5_EXTRA, 201, 276, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B4, 250, 353, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B3, 353, 500, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B2, 500, 707, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B1, 707, 1000, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_B0, 1000, 1414, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C10, 28, 40, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C9, 40, 57, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C8, 57, 81, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C7, 81, 114, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C7C6, 81, 162, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C6, 114, 162, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C6C5, 114, 229, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C5, 162, 229, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C4, 229, 324, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C3, 324, 458, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C2, 458, 648, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C1, 648, 917, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_C0, 917, 1297, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_DL, 110, 220, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_RA2, 430, 610, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_SRA2, 450, 640, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_RA1, 610, 860, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_SRA1, 640, 900, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_RA0, 860, 1220, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::ISO_SRA0, 900, 1280, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B10, 32, 45, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B9, 45, 64, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B8, 64, 91, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B7, 91, 128, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B6, 128, 182, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B5, 182, 257, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B4, 257, 364, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B3, 364, 515, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B2, 515, 728, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B1, 728, 1030, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_B0, 1030, 1456, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JIS_EXEC, 216, 330, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JPN_CHOU4, 90, 205, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JPN_HAGAKI, 100, 148, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JPN_YOU4, 105, 235, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JPN_CHOU2, 111.1f, 146, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JPN_CHOU3, 120, 235, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JPN_OUFUKU, 148, 200, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JPN_KAHU, 240, 322.1f, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::JPN_KAKU2, 240, 332, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_SMALL_PHOTO, 100, 150, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_ITALIAN, 110, 230, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_POSTFIX, 114, 229, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_LARGE_PHOTO, 200, 300, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_FOLIO, 210, 330, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_FOLIO_SP, 215, 315, kMmToUm),
    MAP_CLOUD_PRINT_MEDIA_SIZE(MediaSize::OM_INVITE, 220, 220, kMmToUm)};
#undef MAP_CLOUD_PRINT_MEDIA_SIZE

const gfx::Size& FindMediaSizeByType(MediaSize size_name) {
  for (const auto& media : kMediaDefinitions) {
    if (media.id == size_name) {
      return media.size_um;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return kMediaDefinitions[0].size_um;
}

const MediaDefinition* FindMediaBySize(const gfx::Size& size_um) {
  const MediaDefinition* result = nullptr;
  for (const auto& media : kMediaDefinitions) {
    int32_t diff =
        std::max(std::abs(size_um.width() - media.size_um.width()),
                 std::abs(size_um.height() - media.size_um.height()));
    if (diff < kSizeThresholdUm)
      result = &media;
  }
  return result;
}

template <class T, class IdType>
std::string TypeToString(const T& names, IdType id) {
  for (const auto& name : names) {
    if (id == name.id)
      return name.json_name;
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

template <class T, class IdType>
bool TypeFromString(const T& names, const std::string& type, IdType* id) {
  for (const auto& name : names) {
    if (type == name.json_name) {
      *id = name.id;
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

PwgRasterConfig::~PwgRasterConfig() = default;

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
  NOTREACHED_IN_MIGRATION() << "Bad range capability value type";
  return false;
}

bool RangeVendorCapability::LoadFrom(const base::Value::Dict& dict) {
  const std::string* value_type_str = dict.FindString(kKeyValueType);
  if (!value_type_str || !TypeFromString(kRangeVendorCapabilityTypeNames,
                                         *value_type_str, &value_type_)) {
    return false;
  }
  const std::string* min_value_str = dict.FindString(kMinValue);
  if (!min_value_str)
    return false;
  min_value_ = *min_value_str;
  const std::string* max_value_str = dict.FindString(kMaxValue);
  if (!max_value_str)
    return false;
  max_value_ = *max_value_str;
  const std::string* default_value_str = dict.FindString(kDefaultValue);
  if (default_value_str)
    default_value_ = *default_value_str;
  return IsValid();
}

void RangeVendorCapability::SaveTo(base::Value::Dict* dict) const {
  DCHECK(IsValid());
  dict->Set(kKeyValueType,
            TypeToString(kRangeVendorCapabilityTypeNames, value_type_));
  dict->Set(kMinValue, min_value_);
  dict->Set(kMaxValue, max_value_);
  if (!default_value_.empty())
    dict->Set(kDefaultValue, default_value_);
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
  NOTREACHED_IN_MIGRATION() << "Bad typed value capability value type";
  return false;
}

bool TypedValueVendorCapability::LoadFrom(const base::Value::Dict& dict) {
  const std::string* value_type_str = dict.FindString(kKeyValueType);
  if (!value_type_str || !TypeFromString(kTypedValueVendorCapabilityTypeNames,
                                         *value_type_str, &value_type_)) {
    return false;
  }
  const std::string* default_value_str = dict.FindString(kDefaultValue);
  if (default_value_str)
    default_value_ = *default_value_str;
  return IsValid();
}

void TypedValueVendorCapability::SaveTo(base::Value::Dict* dict) const {
  DCHECK(IsValid());
  dict->Set(kKeyValueType,
            TypeToString(kTypedValueVendorCapabilityTypeNames, value_type_));
  if (!default_value_.empty())
    dict->Set(kDefaultValue, default_value_);
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION() << "Bad vendor capability type";
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
  NOTREACHED_IN_MIGRATION() << "Bad vendor capability type";
  return false;
}

bool VendorCapability::LoadFrom(const base::Value::Dict& dict) {
  InternalCleanup();
  const std::string* type_str = dict.FindString(kKeyType);
  Type type;
  if (!type_str ||
      !TypeFromString(kVendorCapabilityTypeNames, *type_str, &type)) {
    return false;
  }

  const std::string* id_str = dict.FindString(kKeyId);
  if (!id_str)
    return false;

  id_ = *id_str;
  const std::string* display_name_str = dict.FindString(kKeyDisplayName);
  if (!display_name_str)
    return false;

  display_name_ = *display_name_str;
  const base::Value::Dict* range_capability_value =
      dict.FindDict(kOptionRangeCapability);
  if (!range_capability_value == (type == Type::RANGE))
    return false;

  const base::Value::Dict* select_capability_value =
      dict.FindDict(kOptionSelectCapability);
  if (!select_capability_value == (type == Type::SELECT))
    return false;

  const base::Value::Dict* typed_value_capability_value =
      dict.FindDict(kOptionTypedValueCapability);
  if (!typed_value_capability_value == (type == Type::TYPED_VALUE))
    return false;

  type_ = type;
  switch (type_) {
    case Type::NONE:
    default:
      NOTREACHED_IN_MIGRATION();
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

void VendorCapability::SaveTo(base::Value::Dict* dict) const {
  DCHECK(IsValid());
  dict->Set(kKeyType, TypeToString(kVendorCapabilityTypeNames, type_));
  dict->Set(kKeyId, id_);
  dict->Set(kKeyDisplayName, display_name_);

  switch (type_) {
    case Type::NONE:
      NOTREACHED_IN_MIGRATION();
      break;
    case Type::RANGE: {
      base::Value::Dict range_capability_value;
      range_capability_.SaveTo(&range_capability_value);
      dict->Set(kOptionRangeCapability, std::move(range_capability_value));
      break;
    }
    case Type::SELECT: {
      base::Value::Dict select_capability_value;
      select_capability_.SaveTo(&select_capability_value);
      dict->Set(kOptionSelectCapability, std::move(select_capability_value));
      break;
    }
    case Type::TYPED_VALUE: {
      base::Value::Dict typed_value_capability_value;
      typed_value_capability_.SaveTo(&typed_value_capability_value);
      dict->Set(kOptionTypedValueCapability,
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

VendorItem::VendorItem() = default;

VendorItem::VendorItem(const std::string& id, const std::string& value)
    : id(id), value(value) {}

bool VendorItem::IsValid() const {
  return !id.empty() && !value.empty();
}

bool VendorItem::operator==(const VendorItem& other) const {
  return id == other.id && value == other.value;
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
    : size_name(MediaSize::CUSTOM_MEDIA),
      is_continuous_feed(false),
      max_height_um(0),
      has_borderless_variant(false) {}

Media::Media(const Media& other) = default;

Media& Media::operator=(const Media& other) = default;

bool Media::IsValid() const {
  if (size_um.width() <= 0 || size_um.height() <= 0) {
    return false;
  }

  if (is_continuous_feed) {
    if (max_height_um <= size_um.height()) {
      return false;
    }
  }

  if (!gfx::Rect(size_um).Contains(printable_area_um)) {
    return false;
  }

  return true;
}

bool Media::operator==(const Media& other) const {
  return size_name == other.size_name && size_um == other.size_um &&
         is_continuous_feed == other.is_continuous_feed &&
         printable_area_um == other.printable_area_um &&
         max_height_um == other.max_height_um;
}

MediaBuilder::MediaBuilder() = default;

MediaBuilder& MediaBuilder::WithStandardName(MediaSize size_name) {
  size_name_ = size_name;
  custom_display_name_.clear();
  vendor_id_.clear();
  return *this;
}

MediaBuilder& MediaBuilder::WithCustomName(
    const std::string& custom_display_name,
    const std::string& vendor_id) {
  size_name_ = MediaSize::CUSTOM_MEDIA;
  custom_display_name_ = custom_display_name;
  vendor_id_ = vendor_id;
  return *this;
}

MediaBuilder& MediaBuilder::WithSizeAndDefaultPrintableArea(
    const gfx::Size& size_um) {
  return WithSizeAndPrintableArea(size_um, gfx::Rect(size_um));
}

MediaBuilder& MediaBuilder::WithSizeAndPrintableArea(
    const gfx::Size& size_um,
    const gfx::Rect& printable_area_um) {
  size_um_ = size_um;
  printable_area_um_ = printable_area_um;
  return *this;
}

MediaBuilder& MediaBuilder::WithNameMaybeBasedOnSize(
    const std::string& custom_display_name,
    const std::string& vendor_id) {
  WithCustomName(custom_display_name, vendor_id);
  const MediaDefinition* media = FindMediaBySize(size_um_);
  if (media) {
    size_name_ = media->id;
  }
  return *this;
}

MediaBuilder& MediaBuilder::WithSizeAndPrintableAreaBasedOnStandardName() {
  return WithSizeAndDefaultPrintableArea(FindMediaSizeByType(size_name_));
}

MediaBuilder& MediaBuilder::WithMaxHeight(int max_height_um) {
  max_height_um_ = max_height_um;
  return *this;
}

MediaBuilder& MediaBuilder::WithBorderlessVariant(bool has_borderless_variant) {
  has_borderless_variant_ = has_borderless_variant;
  return *this;
}

Media MediaBuilder::Build() const {
  Media result;
  result.size_name = size_name_;
  result.size_um = size_um_;
  result.is_continuous_feed = IsContinuousFeed();
  result.custom_display_name = custom_display_name_;
  result.vendor_id = vendor_id_;
  result.printable_area_um = printable_area_um_;
  result.max_height_um = max_height_um_;
  result.has_borderless_variant = has_borderless_variant_;
  return result;
}

bool MediaBuilder::IsContinuousFeed() const {
  return max_height_um_ > 0;
}

Interval::Interval() : start(0), end(0) {
}

Interval::Interval(int32_t start, int32_t end) : start(start), end(end) {}

Interval::Interval(int32_t start) : start(start), end(kMaxPageNumber) {}

bool Interval::operator==(const Interval& other) const {
  return start == other.start && end == other.end;
}

MediaType::MediaType() = default;

MediaType::MediaType(const std::string& vendor_id,
                     const std::string& custom_display_name)
    : vendor_id(vendor_id), custom_display_name(custom_display_name) {}

bool MediaType::operator==(const MediaType& other) const = default;

bool MediaType::IsValid() const {
  return !vendor_id.empty();
}

template <const char* kName>
class ItemsTraits {
 public:
  static std::string GetCapabilityPath() {
    return base::JoinString({kSectionPrinter, kName}, ".");
  }

  static std::string GetTicketItemPath() {
    return base::JoinString({kSectionPrint, kName}, ".");
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
  static bool Load(const base::Value::Dict& dict, ContentType* option) {
    const std::string* content_type = dict.FindString(kKeyContentType);
    if (!content_type)
      return false;
    *option = *content_type;
    return true;
  }

  static void Save(ContentType option, base::Value::Dict* dict) {
    dict->Set(kKeyContentType, option);
  }
};

class PwgRasterConfigTraits : public NoValueValidation,
                              public ItemsTraits<kOptionPwgRasterConfig> {
 public:
  static bool Load(const base::Value::Dict& dict, PwgRasterConfig* option) {
    PwgRasterConfig option_out;
    const base::Value* document_sheet_back =
        dict.Find(kPwgRasterDocumentSheetBack);
    if (document_sheet_back) {
      if (!document_sheet_back->is_string() ||
          !TypeFromString(kDocumentSheetBackNames,
                          document_sheet_back->GetString(),
                          &option_out.document_sheet_back)) {
        return false;
      }
    }

    const base::Value* document_types_supported =
        dict.Find(kPwgRasterDocumentTypeSupported);
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
        dict.FindBool(kPwgRasterReverseOrderStreaming).value_or(false);
    option_out.rotate_all_pages =
        dict.FindBool(kPwgRasterRotateAllPages).value_or(false);
    *option = option_out;
    return true;
  }

  static void Save(const PwgRasterConfig& option, base::Value::Dict* dict) {
    dict->Set(
        kPwgRasterDocumentSheetBack,
        TypeToString(kDocumentSheetBackNames, option.document_sheet_back));

    if (!option.document_types_supported.empty()) {
      base::Value::List supported_list;
      for (const auto& type : option.document_types_supported) {
        switch (type) {
          case PwgDocumentTypeSupported::SRGB_8:
            supported_list.Append(kTypeDocumentSupportedTypeSRGB8);
            break;
          case PwgDocumentTypeSupported::SGRAY_8:
            supported_list.Append(kTypeDocumentSupportedTypeSGRAY8);
            break;
        }
      }
      dict->Set(kPwgRasterDocumentTypeSupported, std::move(supported_list));
    }

    if (option.reverse_order_streaming) {
      dict->Set(kPwgRasterReverseOrderStreaming,
                option.reverse_order_streaming);
    }

    if (option.rotate_all_pages) {
      dict->Set(kPwgRasterRotateAllPages, option.rotate_all_pages);
    }
  }
};

class VendorCapabilityTraits : public ItemsTraits<kOptionVendorCapability> {
 public:
  static bool IsValid(const VendorCapability& option) {
    return option.IsValid();
  }

  static bool Load(const base::Value::Dict& dict, VendorCapability* option) {
    return option->LoadFrom(dict);
  }

  static void Save(const VendorCapability& option, base::Value::Dict* dict) {
    option.SaveTo(dict);
  }
};

class SelectVendorCapabilityTraits
    : public ItemsTraits<kOptionSelectCapability> {
 public:
  static bool IsValid(const SelectVendorCapabilityOption& option) {
    return option.IsValid();
  }

  static bool Load(const base::Value::Dict& dict,
                   SelectVendorCapabilityOption* option) {
    const std::string* value = dict.FindString(kKeyValue);
    if (!value)
      return false;
    option->value = *value;
    const std::string* display_name = dict.FindString(kKeyDisplayName);
    if (!display_name)
      return false;
    option->display_name = *display_name;
    return true;
  }

  static void Save(const SelectVendorCapabilityOption& option,
                   base::Value::Dict* dict) {
    dict->Set(kKeyValue, option.value);
    dict->Set(kKeyDisplayName, option.display_name);
  }
};

class ColorTraits : public ItemsTraits<kOptionColor> {
 public:
  static bool IsValid(const Color& option) { return option.IsValid(); }

  static bool Load(const base::Value::Dict& dict, Color* option) {
    const std::string* type = dict.FindString(kKeyType);
    if (!type || !TypeFromString(kColorNames, *type, &option->type))
      return false;
    const std::string* vendor_id = dict.FindString(kKeyVendorId);
    if (vendor_id)
      option->vendor_id = *vendor_id;
    const std::string* custom_display_name =
        dict.FindString(kKeyCustomDisplayName);
    if (custom_display_name)
      option->custom_display_name = *custom_display_name;
    return true;
  }

  static void Save(const Color& option, base::Value::Dict* dict) {
    dict->Set(kKeyType, TypeToString(kColorNames, option.type));
    if (!option.vendor_id.empty())
      dict->Set(kKeyVendorId, option.vendor_id);
    if (!option.custom_display_name.empty()) {
      dict->Set(kKeyCustomDisplayName, option.custom_display_name);
    }
  }
};

class DuplexTraits : public NoValueValidation,
                     public ItemsTraits<kOptionDuplex> {
 public:
  static bool Load(const base::Value::Dict& dict, DuplexType* option) {
    const std::string* type = dict.FindString(kKeyType);
    return type && TypeFromString(kDuplexNames, *type, option);
  }

  static void Save(DuplexType option, base::Value::Dict* dict) {
    dict->Set(kKeyType, TypeToString(kDuplexNames, option));
  }
};

class OrientationTraits : public NoValueValidation,
                          public ItemsTraits<kOptionPageOrientation> {
 public:
  static bool Load(const base::Value::Dict& dict, OrientationType* option) {
    const std::string* type = dict.FindString(kKeyType);
    return type && TypeFromString(kOrientationNames, *type, option);
  }

  static void Save(OrientationType option, base::Value::Dict* dict) {
    dict->Set(kKeyType, TypeToString(kOrientationNames, option));
  }
};

class CopiesTicketItemTraits : public NoValueValidation,
                               public ItemsTraits<kOptionCopies> {
 public:
  static bool Load(const base::Value::Dict& dict, int32_t* option) {
    std::optional<int> copies = dict.FindInt(kOptionCopies);
    if (!copies)
      return false;

    *option = copies.value();
    return true;
  }

  static void Save(int32_t option, base::Value::Dict* dict) {
    dict->Set(kOptionCopies, option);
  }
};

class CopiesCapabilityTraits : public NoValueValidation,
                               public ItemsTraits<kOptionCopies> {
 public:
  static bool Load(const base::Value::Dict& dict, Copies* option) {
    std::optional<int> default_copies = dict.FindInt(kDefaultValue);
    if (!default_copies)
      return false;

    std::optional<int> max_copies = dict.FindInt(kMaxValue);
    if (!max_copies)
      return false;

    option->default_value = default_copies.value();
    option->max_value = max_copies.value();
    return true;
  }

  static void Save(const Copies& option, base::Value::Dict* dict) {
    dict->Set(kDefaultValue, option.default_value);
    dict->Set(kMaxValue, option.max_value);
  }
};

class MarginsTraits : public NoValueValidation,
                      public ItemsTraits<kOptionMargins> {
 public:
  static bool Load(const base::Value::Dict& dict, Margins* option) {
    const std::string* type = dict.FindString(kKeyType);
    if (!type || !TypeFromString(kMarginsNames, *type, &option->type))
      return false;
    std::optional<int> top_um = dict.FindInt(kMarginTop);
    std::optional<int> right_um = dict.FindInt(kMarginRight);
    std::optional<int> bottom_um = dict.FindInt(kMarginBottom);
    std::optional<int> left_um = dict.FindInt(kMarginLeft);
    if (!top_um || !right_um || !bottom_um || !left_um)
      return false;
    option->top_um = top_um.value();
    option->right_um = right_um.value();
    option->bottom_um = bottom_um.value();
    option->left_um = left_um.value();
    return true;
  }

  static void Save(const Margins& option, base::Value::Dict* dict) {
    dict->Set(kKeyType, TypeToString(kMarginsNames, option.type));
    dict->Set(kMarginTop, option.top_um);
    dict->Set(kMarginRight, option.right_um);
    dict->Set(kMarginBottom, option.bottom_um);
    dict->Set(kMarginLeft, option.left_um);
  }
};

class DpiTraits : public ItemsTraits<kOptionDpi> {
 public:
  static bool IsValid(const Dpi& option) { return option.IsValid(); }

  static bool Load(const base::Value::Dict& dict, Dpi* option) {
    std::optional<int> horizontal = dict.FindInt(kDpiHorizontal);
    std::optional<int> vertical = dict.FindInt(kDpiVertical);
    if (!horizontal || !vertical)
      return false;
    option->horizontal = horizontal.value();
    option->vertical = vertical.value();
    return true;
  }

  static void Save(const Dpi& option, base::Value::Dict* dict) {
    dict->Set(kDpiHorizontal, option.horizontal);
    dict->Set(kDpiVertical, option.vertical);
  }
};

class FitToPageTraits : public NoValueValidation,
                        public ItemsTraits<kOptionFitToPage> {
 public:
  static bool Load(const base::Value::Dict& dict, FitToPageType* option) {
    const std::string* type = dict.FindString(kKeyType);
    return type && TypeFromString(kFitToPageNames, *type, option);
  }

  static void Save(FitToPageType option, base::Value::Dict* dict) {
    dict->Set(kKeyType, TypeToString(kFitToPageNames, option));
  }
};

class PageRangeTraits : public ItemsTraits<kOptionPageRange> {
 public:
  static bool IsValid(const PageRange& option) {
    for (const auto& item : option) {
      if (item.start < 1 || item.end < 1) {
        return false;
      }
    }
    return true;
  }

  static bool Load(const base::Value::Dict& dict, PageRange* option) {
    const base::Value::List* list_value = dict.FindList(kPageRangeInterval);
    if (!list_value)
      return false;
    for (const base::Value& interval : *list_value) {
      const auto& inverval_dict = interval.GetDict();
      int page_range_start = inverval_dict.FindInt(kPageRangeStart).value_or(1);
      int page_range_end =
          inverval_dict.FindInt(kPageRangeEnd).value_or(kMaxPageNumber);
      option->push_back(Interval(page_range_start, page_range_end));
    }
    return true;
  }

  static void Save(const PageRange& option, base::Value::Dict* dict) {
    if (!option.empty()) {
      base::Value::List list;
      for (const auto& item : option) {
        base::Value::Dict interval;
        interval.Set(kPageRangeStart, item.start);
        if (item.end < kMaxPageNumber)
          interval.Set(kPageRangeEnd, item.end);
        list.Append(std::move(interval));
      }
      dict->Set(kPageRangeInterval, std::move(list));
    }
  }
};

class MediaTraits : public ItemsTraits<kOptionMediaSize> {
 public:
  static bool IsValid(const Media& option) { return option.IsValid(); }

  static bool Load(const base::Value::Dict& dict, Media* option) {
    const std::string* type = dict.FindString(kKeyName);
    if (type && !TypeFromString(kMediaDefinitions, *type, &option->size_name)) {
      return false;
    }
    const std::string* custom_display_name =
        dict.FindString(kKeyCustomDisplayName);
    if (custom_display_name)
      option->custom_display_name = *custom_display_name;
    const std::string* vendor_id = dict.FindString(kKeyVendorId);
    if (vendor_id)
      option->vendor_id = *vendor_id;
    std::optional<int> width_um = dict.FindInt(kMediaWidth);
    if (width_um) {
      option->size_um.set_width(width_um.value());
    }
    std::optional<bool> is_continuous_feed = dict.FindBool(kMediaIsContinuous);
    if (is_continuous_feed) {
      option->is_continuous_feed = is_continuous_feed.value();
    }
    if (is_continuous_feed.value_or(false)) {
      // The min/max height is required for continuous feed media.
      std::optional<int> min_height_um = dict.FindInt(kMediaMinHeight);
      std::optional<int> max_height_um = dict.FindInt(kMediaMaxHeight);
      if (!min_height_um || !max_height_um) {
        return false;
      }
      // For variable height media, the min height is stored in the height
      // attribute of the `size_um` parameter.
      option->size_um.set_height(min_height_um.value());
      option->max_height_um = max_height_um.value();

      // When `option` is a continuous feed, the printable area is not
      // applicable. For consistency with the constructors, set the printable
      // area to the default page size value.
      option->printable_area_um = gfx::Rect(option->size_um);
      return true;
    }
    std::optional<int> height_um = dict.FindInt(kMediaHeight);
    if (height_um) {
      option->size_um.set_height(height_um.value());
    }
    std::optional<int> imageable_area_left =
        dict.FindInt(kMediaImageableAreaLeft);
    std::optional<int> imageable_area_bottom =
        dict.FindInt(kMediaImageableAreaBottom);
    std::optional<int> imageable_area_right =
        dict.FindInt(kMediaImageableAreaRight);
    std::optional<int> imageable_area_top =
        dict.FindInt(kMediaImageableAreaTop);
    if (imageable_area_left && imageable_area_bottom && imageable_area_right &&
        imageable_area_top) {
      int width = imageable_area_right.value() - imageable_area_left.value();
      int height = imageable_area_top.value() - imageable_area_bottom.value();
      option->printable_area_um =
          gfx::Rect(imageable_area_left.value(), imageable_area_bottom.value(),
                    width, height);
    }

    std::optional<bool> has_borderless_variant =
        dict.FindBool(kMediaHasBorderlessVariant);
    if (has_borderless_variant) {
      option->has_borderless_variant = has_borderless_variant.value();
    }

    return true;
  }

  static void Save(const Media& option, base::Value::Dict* dict) {
    if (option.size_name != MediaSize::CUSTOM_MEDIA) {
      dict->Set(kKeyName, TypeToString(kMediaDefinitions, option.size_name));
    }
    if (!option.custom_display_name.empty() ||
        option.size_name == MediaSize::CUSTOM_MEDIA) {
      dict->Set(kKeyCustomDisplayName, option.custom_display_name);
    }
    if (!option.vendor_id.empty())
      dict->Set(kKeyVendorId, option.vendor_id);
    if (option.size_um.width() > 0)
      dict->Set(kMediaWidth, option.size_um.width());
    if (option.is_continuous_feed) {
      // For variable height media, the height from `size_um` represents the min
      // height, so it gets stored in `kMediaMinHeight`, not in `kMediaHeight`.
      dict->Set(kMediaIsContinuous, true);
      dict->Set(kMediaMinHeight, option.size_um.height());
      dict->Set(kMediaMaxHeight, option.max_height_um);
    } else if (option.size_um.height() > 0) {
      dict->Set(kMediaHeight, option.size_um.height());
    }
    if (!option.is_continuous_feed && !option.printable_area_um.IsEmpty() &&
        gfx::Rect(option.size_um).Contains(option.printable_area_um)) {
      dict->Set(kMediaImageableAreaLeft, option.printable_area_um.x());
      dict->Set(kMediaImageableAreaBottom, option.printable_area_um.y());
      dict->Set(kMediaImageableAreaRight, option.printable_area_um.x() +
                                              option.printable_area_um.width());
      dict->Set(kMediaImageableAreaTop, option.printable_area_um.y() +
                                            option.printable_area_um.height());
    }
    if (option.has_borderless_variant) {
      dict->Set(kMediaHasBorderlessVariant, true);
    }
  }
};

class MediaTypeTraits : public ItemsTraits<kOptionMediaType> {
 public:
  static bool IsValid(const MediaType& option) { return option.IsValid(); }

  static bool Load(const base::Value::Dict& dict, MediaType* option) {
    const std::string* vendor_id = dict.FindString(kKeyVendorId);
    if (!vendor_id) {
      return false;
    }
    option->vendor_id = *vendor_id;
    const std::string* custom_display_name =
        dict.FindString(kKeyCustomDisplayName);
    if (custom_display_name) {
      option->custom_display_name = *custom_display_name;
    }
    return true;
  }

  static void Save(const MediaType& option, base::Value::Dict* dict) {
    dict->Set(kKeyVendorId, option.vendor_id);
    if (!option.custom_display_name.empty()) {
      dict->Set(kKeyCustomDisplayName, option.custom_display_name);
    }
  }
};

class CollateTraits : public NoValueValidation,
                      public ItemsTraits<kOptionCollate> {
 public:
  static const bool kDefault = true;

  static bool Load(const base::Value::Dict& dict, bool* option) {
    std::optional<bool> collate = dict.FindBool(kOptionCollate);
    if (!collate)
      return false;
    *option = collate.value();
    return true;
  }

  static void Save(bool option, base::Value::Dict* dict) {
    dict->Set(kOptionCollate, option);
  }
};

class ReverseTraits : public NoValueValidation,
                      public ItemsTraits<kOptionReverse> {
 public:
  static const bool kDefault = false;

  static bool Load(const base::Value::Dict& dict, bool* option) {
    std::optional<bool> reverse = dict.FindBool(kOptionReverse);
    if (!reverse)
      return false;
    *option = reverse.value();
    return true;
  }

  static void Save(bool option, base::Value::Dict* dict) {
    dict->Set(kOptionReverse, option);
  }
};

class VendorItemTraits : public ItemsTraits<kOptionVendorItem> {
 public:
  static bool IsValid(const VendorItem& option) { return option.IsValid(); }

  static bool Load(const base::Value::Dict& dict, VendorItem* option) {
    const std::string* id = dict.FindString(kKeyId);
    if (!id) {
      return false;
    }
    const std::string* value = dict.FindString(kKeyValue);
    if (!value) {
      return false;
    }
    option->id = *id;
    option->value = *value;
    return true;
  }

  static void Save(const VendorItem& option, base::Value::Dict* dict) {
    dict->Set(kKeyId, option.id);
    dict->Set(kKeyValue, option.value);
  }
};

#if BUILDFLAG(IS_CHROMEOS)
class PinTraits : public NoValueValidation, public ItemsTraits<kOptionPin> {
 public:
  static bool Load(const base::Value::Dict& dict, bool* option) {
    std::optional<bool> supported = dict.FindBool(kPinSupported);
    if (!supported)
      return false;
    *option = supported.value();
    return true;
  }

  static void Save(bool option, base::Value::Dict* dict) {
    dict->Set(kPinSupported, option);
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS)

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
template class SelectionCapability<printer::MediaType,
                                   printer::MediaTypeTraits>;
template class ValueCapability<printer::Copies,
                               printer::CopiesCapabilityTraits>;
template class EmptyCapability<printer::PageRangeTraits>;
template class BooleanCapability<printer::CollateTraits>;
template class BooleanCapability<printer::ReverseTraits>;
#if BUILDFLAG(IS_CHROMEOS)
template class ValueCapability<bool, printer::PinTraits>;
#endif  // BUILDFLAG(IS_CHROMEOS)

template class TicketItem<printer::PwgRasterConfig,
                          printer::PwgRasterConfigTraits>;
template class TicketItem<printer::Color, printer::ColorTraits>;
template class TicketItem<printer::DuplexType, printer::DuplexTraits>;
template class TicketItem<printer::OrientationType, printer::OrientationTraits>;
template class TicketItem<printer::Margins, printer::MarginsTraits>;
template class TicketItem<printer::Dpi, printer::DpiTraits>;
template class TicketItem<printer::FitToPageType, printer::FitToPageTraits>;
template class TicketItem<printer::Media, printer::MediaTraits>;
template class TicketItem<int32_t, printer::CopiesTicketItemTraits>;
template class TicketItem<printer::PageRange, printer::PageRangeTraits>;
template class TicketItem<bool, printer::CollateTraits>;
template class TicketItem<bool, printer::ReverseTraits>;
template class ListCapability<printer::VendorItem, printer::VendorItemTraits>;
template class ListTicketItem<printer::VendorItem, printer::VendorItemTraits>;

}  // namespace cloud_devices
