// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLOUD_DEVICES_COMMON_PRINTER_DESCRIPTION_H_
#define COMPONENTS_CLOUD_DEVICES_COMMON_PRINTER_DESCRIPTION_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/cloud_devices/common/description_items.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// Defines printer options, CDD and CJT items.
// https://developers.google.com/cloud-print/docs/cdd

namespace cloud_devices {

namespace printer {

struct SelectVendorCapabilityOption;
class SelectVendorCapabilityTraits;
typedef SelectionCapability<SelectVendorCapabilityOption,
                            SelectVendorCapabilityTraits>
    SelectVendorCapability;

typedef std::string ContentType;

struct Copies {
  // Default requested number of copies.
  int32_t default_value = 1;

  // Maximum number of copies supported, sourced from
  // PrinterSemanticCapsAndDefaults.copies_max.
  int32_t max_value = 1;
};

enum class DocumentSheetBack {
  NORMAL,
  ROTATED,
  MANUAL_TUMBLE,
  FLIPPED,
};

enum class PwgDocumentTypeSupported {
  SGRAY_8 = 22,
  SRGB_8 = 23,
};

struct PwgRasterConfig {
  PwgRasterConfig();
  ~PwgRasterConfig();

  std::vector<PwgDocumentTypeSupported> document_types_supported;
  DocumentSheetBack document_sheet_back;
  bool reverse_order_streaming;
  bool rotate_all_pages;
};

class RangeVendorCapability {
 public:
  enum class ValueType {
    FLOAT,
    INTEGER,
  };

  RangeVendorCapability();
  RangeVendorCapability(ValueType value_type,
                        const std::string& min_value,
                        const std::string& max_value);
  RangeVendorCapability(ValueType value_type,
                        const std::string& min_value,
                        const std::string& max_value,
                        const std::string& default_value);
  RangeVendorCapability(RangeVendorCapability&& other);

  RangeVendorCapability(const RangeVendorCapability&) = delete;
  RangeVendorCapability& operator=(const RangeVendorCapability&) = delete;

  ~RangeVendorCapability();

  RangeVendorCapability& operator=(RangeVendorCapability&& other);

  bool operator==(const RangeVendorCapability& other) const;
  bool operator!=(const RangeVendorCapability& other) const {
    return !(*this == other);
  }

  bool IsValid() const;
  bool LoadFrom(const base::Value::Dict& dict);
  void SaveTo(base::Value::Dict* dict) const;

 private:
  ValueType value_type_;
  std::string min_value_;
  std::string max_value_;
  std::string default_value_;
};

struct SelectVendorCapabilityOption {
  SelectVendorCapabilityOption();
  SelectVendorCapabilityOption(const std::string& value,
                               const std::string& display_name);
  ~SelectVendorCapabilityOption();

  bool IsValid() const;
  bool operator==(const SelectVendorCapabilityOption& other) const;
  bool operator!=(const SelectVendorCapabilityOption& other) const {
    return !(*this == other);
  }

  std::string value;
  std::string display_name;
};

class TypedValueVendorCapability {
 public:
  enum class ValueType {
    BOOLEAN,
    FLOAT,
    INTEGER,
    STRING,
  };

  TypedValueVendorCapability();
  explicit TypedValueVendorCapability(ValueType value_type);
  TypedValueVendorCapability(ValueType value_type,
                             const std::string& default_value);
  TypedValueVendorCapability(TypedValueVendorCapability&& other);

  TypedValueVendorCapability(const TypedValueVendorCapability&) = delete;
  TypedValueVendorCapability& operator=(const TypedValueVendorCapability&) =
      delete;

  ~TypedValueVendorCapability();

  TypedValueVendorCapability& operator=(TypedValueVendorCapability&& other);

  bool operator==(const TypedValueVendorCapability& other) const;
  bool operator!=(const TypedValueVendorCapability& other) const {
    return !(*this == other);
  }

  bool IsValid() const;
  bool LoadFrom(const base::Value::Dict& dict);
  void SaveTo(base::Value::Dict* dict) const;

 private:
  ValueType value_type_;
  std::string default_value_;
};

class VendorCapability {
 public:
  enum class Type {
    NONE,
    RANGE,
    SELECT,
    TYPED_VALUE,
  };

  VendorCapability();
  VendorCapability(const std::string& id,
                   const std::string& display_name,
                   RangeVendorCapability range_capability);
  VendorCapability(const std::string& id,
                   const std::string& display_name,
                   SelectVendorCapability select_capability);
  VendorCapability(const std::string& id,
                   const std::string& display_name,
                   TypedValueVendorCapability typed_value_capability);
  VendorCapability(VendorCapability&& other);

  VendorCapability(const VendorCapability&) = delete;
  VendorCapability& operator=(const VendorCapability&) = delete;

  ~VendorCapability();

  bool operator==(const VendorCapability& other) const;
  bool operator!=(const VendorCapability& other) const {
    return !(*this == other);
  }

  bool IsValid() const;
  bool LoadFrom(const base::Value::Dict& dict);
  void SaveTo(base::Value::Dict* dict) const;

 private:
  void InternalCleanup();

  Type type_;
  std::string id_;
  std::string display_name_;

  // If the CDD is valid, exactly one of the capabilities has a value.
  union {
    RangeVendorCapability range_capability_;
    SelectVendorCapability select_capability_;
    TypedValueVendorCapability typed_value_capability_;
  };
};

struct VendorItem {
  VendorItem();
  VendorItem(const std::string& id, const std::string& value);

  bool IsValid() const;
  bool operator==(const VendorItem& other) const;
  bool operator!=(const VendorItem& other) const { return !(*this == other); }

  std::string id;
  std::string value;
};

enum class ColorType {
  STANDARD_COLOR,
  STANDARD_MONOCHROME,
  CUSTOM_COLOR,
  CUSTOM_MONOCHROME,
  AUTO_COLOR,
};

struct Color {
  Color();
  explicit Color(ColorType type);

  bool IsValid() const;
  bool operator==(const Color& other) const;
  bool operator!=(const Color& other) const { return !(*this == other); }

  ColorType type;
  std::string vendor_id;
  std::string custom_display_name;
};

enum class DuplexType {
  NO_DUPLEX,
  LONG_EDGE,
  SHORT_EDGE,
};

enum class OrientationType {
  PORTRAIT,
  LANDSCAPE,
  AUTO_ORIENTATION,
};

enum class MarginsType {
  NO_MARGINS,
  STANDARD_MARGINS,
  CUSTOM_MARGINS,
};

struct Margins {
  Margins();
  Margins(MarginsType type,
          int32_t top_um,
          int32_t right_um,
          int32_t bottom_um,
          int32_t left_um);

  bool operator==(const Margins& other) const;
  bool operator!=(const Margins& other) const { return !(*this == other); }

  MarginsType type;
  int32_t top_um;
  int32_t right_um;
  int32_t bottom_um;
  int32_t left_um;
};

struct Dpi {
  Dpi();
  Dpi(int32_t horizontal, int32_t vertical);

  bool IsValid() const;
  bool operator==(const Dpi& other) const;
  bool operator!=(const Dpi& other) const { return !(*this == other); }

  int32_t horizontal;
  int32_t vertical;
};

enum class FitToPageType {
  NO_FITTING,
  FIT_TO_PAGE,
  GROW_TO_PAGE,
  SHRINK_TO_PAGE,
  FILL_PAGE,
};

enum class MediaSize {
  CUSTOM_MEDIA,

  // North American standard sheet media names.
  NA_INDEX_3X5,
  NA_PERSONAL,
  NA_MONARCH,
  NA_NUMBER_9,
  NA_INDEX_4X6,
  NA_NUMBER_10,
  NA_A2,
  NA_NUMBER_11,
  NA_NUMBER_12,
  NA_5X7,
  NA_INDEX_5X8,
  NA_NUMBER_14,
  NA_INVOICE,
  NA_INDEX_4X6_EXT,
  NA_6X9,
  NA_C5,
  NA_7X9,
  NA_EXECUTIVE,
  NA_GOVT_LETTER,
  NA_GOVT_LEGAL,
  NA_QUARTO,
  NA_LETTER,
  NA_FANFOLD_EUR,
  NA_LETTER_PLUS,
  NA_FOOLSCAP,
  NA_LEGAL,
  NA_SUPER_A,
  NA_9X11,
  NA_ARCH_A,
  NA_LETTER_EXTRA,
  NA_LEGAL_EXTRA,
  NA_10X11,
  NA_10X13,
  NA_10X14,
  NA_10X15,
  NA_11X12,
  NA_EDP,
  NA_FANFOLD_US,
  NA_11X15,
  NA_LEDGER,
  NA_EUR_EDP,
  NA_ARCH_B,
  NA_12X19,
  NA_B_PLUS,
  NA_SUPER_B,
  NA_C,
  NA_ARCH_C,
  NA_D,
  NA_ARCH_D,
  NA_ASME_F,
  NA_WIDE_FORMAT,
  NA_E,
  NA_ARCH_E,
  NA_F,

  // Chinese standard sheet media size names.
  ROC_16K,
  ROC_8K,
  PRC_32K,
  PRC_1,
  PRC_2,
  PRC_4,
  PRC_5,
  PRC_8,
  PRC_6,
  PRC_3,
  PRC_16K,
  PRC_7,
  OM_JUURO_KU_KAI,
  OM_PA_KAI,
  OM_DAI_PA_KAI,
  PRC_10,

  // ISO standard sheet media size names.
  ISO_A10,
  ISO_A9,
  ISO_A8,
  ISO_A7,
  ISO_A6,
  ISO_A5,
  ISO_A5_EXTRA,
  ISO_A4,
  ISO_A4_TAB,
  ISO_A4_EXTRA,
  ISO_A3,
  ISO_A4X3,
  ISO_A4X4,
  ISO_A4X5,
  ISO_A4X6,
  ISO_A4X7,
  ISO_A4X8,
  ISO_A4X9,
  ISO_A3_EXTRA,
  ISO_A2,
  ISO_A3X3,
  ISO_A3X4,
  ISO_A3X5,
  ISO_A3X6,
  ISO_A3X7,
  ISO_A1,
  ISO_A2X3,
  ISO_A2X4,
  ISO_A2X5,
  ISO_A0,
  ISO_A1X3,
  ISO_A1X4,
  ISO_2A0,
  ISO_A0X3,
  ISO_B10,
  ISO_B9,
  ISO_B8,
  ISO_B7,
  ISO_B6,
  ISO_B6C4,
  ISO_B5,
  ISO_B5_EXTRA,
  ISO_B4,
  ISO_B3,
  ISO_B2,
  ISO_B1,
  ISO_B0,
  ISO_C10,
  ISO_C9,
  ISO_C8,
  ISO_C7,
  ISO_C7C6,
  ISO_C6,
  ISO_C6C5,
  ISO_C5,
  ISO_C4,
  ISO_C3,
  ISO_C2,
  ISO_C1,
  ISO_C0,
  ISO_DL,
  ISO_RA2,
  ISO_SRA2,
  ISO_RA1,
  ISO_SRA1,
  ISO_RA0,
  ISO_SRA0,

  // Japanese standard sheet media size names.
  JIS_B10,
  JIS_B9,
  JIS_B8,
  JIS_B7,
  JIS_B6,
  JIS_B5,
  JIS_B4,
  JIS_B3,
  JIS_B2,
  JIS_B1,
  JIS_B0,
  JIS_EXEC,
  JPN_CHOU4,
  JPN_HAGAKI,
  JPN_YOU4,
  JPN_CHOU2,
  JPN_CHOU3,
  JPN_OUFUKU,
  JPN_KAHU,
  JPN_KAKU2,

  // Other metric standard sheet media size names.
  OM_SMALL_PHOTO,
  OM_ITALIAN,
  OM_POSTFIX,
  OM_LARGE_PHOTO,
  OM_FOLIO,
  OM_FOLIO_SP,
  OM_INVITE,
};

struct Media {
  // Use `MediaBuilder` below to build struct `Media` in an organized fashion.
  Media();

  Media(const Media& other);
  Media& operator=(const Media& other);

  bool IsValid() const;
  bool operator==(const Media& other) const;
  bool operator!=(const Media& other) const { return !(*this == other); }

  MediaSize size_name;
  gfx::Size size_um;
  bool is_continuous_feed;
  std::string custom_display_name;
  std::string vendor_id;
  gfx::Rect printable_area_um;
  // When `is_continuous_feed` is true, the min height will be stored in
  // `size_um` and the max height is stored in `max_height_um`.  When
  // `is_continuous_feed` is false, `max_height_um` is not used.  This only
  // supports a variable height, not width, so the width is always fixed.
  int max_height_um;
  bool has_borderless_variant;
};

// Builds `Media` structs. The caller must call at least one method that sets
// the name, and follow any special requirements described by the methods below.
//
// Note that the build result can be invalid, e.g. If the caller passes in an
// empty size or forgets to specify the size.
//
// Note that there is no requirement that the size corresponds to the
// `MediaSize`. e.g. `MediaSize::NA_LETTER` and a size of 100x200 is considered
// valid.
class MediaBuilder {
 public:
  MediaBuilder();

  // Sets the name to `size_name` and clears the display name and vendor ID.
  MediaBuilder& WithStandardName(MediaSize size_name);

  // Sets the name to `MediaSize::CUSTOM_MEDIA`. Sets the display name and
  // vendor ID based on the input parameters.
  MediaBuilder& WithCustomName(const std::string& custom_display_name,
                               const std::string& vendor_id);

  // The default printable area is the entire medium.
  MediaBuilder& WithSizeAndDefaultPrintableArea(const gfx::Size& size_um);
  MediaBuilder& WithSizeAndPrintableArea(const gfx::Size& size_um,
                                         const gfx::Rect& printable_area_um);
  MediaBuilder& WithMaxHeight(int max_height_um);
  MediaBuilder& WithBorderlessVariant(bool has_borderless_variant);

  // This is equivalent to calling WithCustomName(), except it also tries to
  // match the name to a standard `MediaSize`.
  // This requires the caller to call WithSizeAndDefaultPrintableArea() or
  // WithSizeAndPrintableArea() first.
  MediaBuilder& WithNameMaybeBasedOnSize(const std::string& custom_display_name,
                                         const std::string& vendor_id);

  // Sets the size and printable area based on the standard name, For example,
  // if the name is `MediaSize::NA_LETTER`, then this sets the size to
  // 215900x279400.
  // This requires the caller to call WithStandardName() first with a name other
  // than `MediaSize::CUSTOM_MEDIA`.
  MediaBuilder& WithSizeAndPrintableAreaBasedOnStandardName();

  // Can be called repeatedly to build `Media` structs.
  Media Build() const;

 private:
  bool IsContinuousFeed() const;

  MediaSize size_name_;
  std::string custom_display_name_;
  std::string vendor_id_;

  gfx::Size size_um_;
  gfx::Rect printable_area_um_;
  int max_height_um_ = 0;
  bool has_borderless_variant_ = false;
};

struct Interval {
  Interval();
  Interval(int32_t start, int32_t end);
  explicit Interval(int32_t start);

  bool operator==(const Interval& other) const;
  bool operator!=(const Interval& other) const { return !(*this == other); }

  int32_t start;
  int32_t end;
};

typedef std::vector<Interval> PageRange;

struct MediaType {
  MediaType();
  MediaType(const std::string& vendor_id,
            const std::string& custom_display_name);

  bool operator==(const MediaType& other) const;
  bool operator!=(const MediaType& other) const { return !(*this == other); }

  bool IsValid() const;

  std::string vendor_id;
  std::string custom_display_name;
};

class ContentTypeTraits;
class PwgRasterConfigTraits;
class VendorCapabilityTraits;
class ColorTraits;
class DuplexTraits;
class OrientationTraits;
class MarginsTraits;
class DpiTraits;
class FitToPageTraits;
class MediaTraits;
class MediaTypeTraits;
class PageRangeTraits;
class CollateTraits;
class CopiesCapabilityTraits;
class CopiesTicketItemTraits;
class VendorItemTraits;

typedef ListCapability<ContentType, ContentTypeTraits> ContentTypesCapability;
typedef ValueCapability<PwgRasterConfig, PwgRasterConfigTraits>
    PwgRasterConfigCapability;
typedef ListCapability<VendorCapability, VendorCapabilityTraits>
    VendorCapabilities;
typedef SelectionCapability<Color, ColorTraits> ColorCapability;
typedef SelectionCapability<DuplexType, DuplexTraits> DuplexCapability;
typedef SelectionCapability<OrientationType, OrientationTraits>
    OrientationCapability;
typedef SelectionCapability<Margins, MarginsTraits> MarginsCapability;
typedef SelectionCapability<Dpi, DpiTraits> DpiCapability;
typedef SelectionCapability<FitToPageType, FitToPageTraits> FitToPageCapability;
typedef SelectionCapability<Media, MediaTraits> MediaCapability;
typedef SelectionCapability<MediaType, MediaTypeTraits> MediaTypeCapability;
typedef ValueCapability<Copies, class CopiesCapabilityTraits> CopiesCapability;
typedef EmptyCapability<class PageRangeTraits> PageRangeCapability;
typedef BooleanCapability<class CollateTraits> CollateCapability;
typedef BooleanCapability<class ReverseTraits> ReverseCapability;
#if BUILDFLAG(IS_CHROMEOS)
// This capability is not a part of standard CDD description. It's used for
// providing PIN printing opportunity in Chrome OS native printing.
typedef ValueCapability<bool, class PinTraits> PinCapability;
#endif  // BUILDFLAG(IS_CHROMEOS)

typedef TicketItem<PwgRasterConfig, PwgRasterConfigTraits>
    PwgRasterConfigTicketItem;
typedef TicketItem<Color, ColorTraits> ColorTicketItem;
typedef TicketItem<DuplexType, DuplexTraits> DuplexTicketItem;
typedef TicketItem<OrientationType, OrientationTraits> OrientationTicketItem;
typedef TicketItem<Margins, MarginsTraits> MarginsTicketItem;
typedef TicketItem<Dpi, DpiTraits> DpiTicketItem;
typedef TicketItem<FitToPageType, FitToPageTraits> FitToPageTicketItem;
typedef TicketItem<Media, MediaTraits> MediaTicketItem;
typedef TicketItem<int32_t, CopiesTicketItemTraits> CopiesTicketItem;
typedef TicketItem<PageRange, PageRangeTraits> PageRangeTicketItem;
typedef TicketItem<bool, CollateTraits> CollateTicketItem;
typedef TicketItem<bool, ReverseTraits> ReverseTicketItem;
typedef ListTicketItem<VendorItem, VendorItemTraits> VendorTicketItems;

}  // namespace printer

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_COMMON_PRINTER_DESCRIPTION_H_
