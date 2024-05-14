// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/cloud_print_cdd_conversion.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/cloud_devices/common/cloud_device_description.h"
#include "components/cloud_devices/common/printer_description.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"

namespace printer = cloud_devices::printer;

namespace cloud_print {

namespace {

#if BUILDFLAG(IS_WIN)
constexpr char kIdPageOutputQuality[] = "page_output_quality";
constexpr char kDisplayNamePageOutputQuality[] = "Page output quality";
#endif  // BUILDFLAG(IS_WIN)

printer::DuplexType ToCloudDuplexType(printing::mojom::DuplexMode mode) {
  switch (mode) {
    case printing::mojom::DuplexMode::kSimplex:
      return printer::DuplexType::NO_DUPLEX;
    case printing::mojom::DuplexMode::kLongEdge:
      return printer::DuplexType::LONG_EDGE;
    case printing::mojom::DuplexMode::kShortEdge:
      return printer::DuplexType::SHORT_EDGE;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return printer::DuplexType::NO_DUPLEX;
}

#if BUILDFLAG(IS_CHROMEOS)
printer::TypedValueVendorCapability::ValueType ToCloudValueType(
    printing::AdvancedCapability::Type type) {
  switch (type) {
    case printing::AdvancedCapability::Type::kBoolean:
      return printer::TypedValueVendorCapability::ValueType::BOOLEAN;
    case printing::AdvancedCapability::Type::kFloat:
      return printer::TypedValueVendorCapability::ValueType::FLOAT;
    case printing::AdvancedCapability::Type::kInteger:
      return printer::TypedValueVendorCapability::ValueType::INTEGER;
    case printing::AdvancedCapability::Type::kString:
      return printer::TypedValueVendorCapability::ValueType::STRING;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return printer::TypedValueVendorCapability::ValueType::STRING;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

printer::Media ConvertPaperToMedia(
    const printing::PrinterSemanticCapsAndDefaults::Paper& paper) {
  if (paper.SupportsCustomSize()) {
    return printer::MediaBuilder()
        .WithCustomName(paper.display_name(), paper.vendor_id())
        .WithSizeAndDefaultPrintableArea(paper.size_um())
        .WithMaxHeight(paper.max_height_um())
        .Build();
  }

  gfx::Size paper_size = paper.size_um();
  gfx::Rect paper_printable_area = paper.printable_area_um();
  // When converting to Media, the size and printable area should have a larger
  // height than width.
  if (paper_size.width() > paper_size.height()) {
    // When swapping the printable_area, we can't simply transpose the rect
    // since the margins may not be the same on all sides.  A visualization may
    // help.  Suppose we have a page with width of 127000 and height of 76200.
    // Additionally, the left margin is 1000, right margin is 700, bottom margin
    // is 500, and top margin is 200.
    //
    //        +---------- 127000 ---------+
    //        |        200                |
    //      76200      +-----------+      |
    //        |        |           |      |
    //        |  1000  |           |  700 |
    //        |        |           |      |
    //        |        +-----------+      |
    //        |         500               |
    //        +---------------------------+
    //
    // After swapping the page size and printable area (rotating 90 degrees
    // clockwise), this should be the resulting sizes:
    //
    //        +---- 76200 ---+
    //        |              |
    //        |      1000    |
    //        |             127000
    //        |    +-----+   |
    //        |    |     |   |
    //        |    |     |   |
    //        |500 |     |200|
    //        |    |     |   |
    //        |    |     |   |
    //        |    +-----+   |
    //        |              |
    //        |      700     |
    //        +--------------+
    //
    // Namely, the new x value for the printable area is: the old printable area
    // y value.  The new y value for the printable area is: (the old width) -
    // (the old printable area width) - (the old printable areay x value).  Note
    // that if the top/bottom margins are equal and the left/right margins are
    // equal, then a simple transpose does indeed work.

    // Rotate clockwise by 90 degrees.
    int new_x = paper_printable_area.y();
    int new_y = paper_size.width() - paper_printable_area.width() -
                paper_printable_area.x();
    paper_printable_area.SetRect(new_x, new_y, paper_printable_area.height(),
                                 paper_printable_area.width());
    paper_size.SetSize(paper_size.height(), paper_size.width());
  }
  return printer::MediaBuilder()
      .WithSizeAndPrintableArea(paper_size, paper_printable_area)
      .WithNameMaybeBasedOnSize(paper.display_name(), paper.vendor_id())
      .WithBorderlessVariant(paper.has_borderless_variant())
      .Build();
}

printer::MediaCapability GetMediaCapabilities(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  printer::MediaCapability media_capabilities;
  bool is_default_set = false;

  const printing::PrinterSemanticCapsAndDefaults::Paper& default_paper =
      semantic_info.default_paper;
  printer::Media default_media =
      printer::MediaBuilder()
          .WithSizeAndPrintableArea(default_paper.size_um(),
                                    default_paper.printable_area_um())
          .WithNameMaybeBasedOnSize(default_paper.display_name(),
                                    default_paper.vendor_id())
          .WithBorderlessVariant(default_paper.has_borderless_variant())
          .Build();

  for (const auto& paper : semantic_info.papers) {
    printer::Media new_media = ConvertPaperToMedia(paper);
    if (!new_media.IsValid())
      continue;

    if (media_capabilities.Contains(new_media))
      continue;

    if (!default_media.IsValid())
      default_media = new_media;
    media_capabilities.AddDefaultOption(new_media, new_media == default_media);
    is_default_set = is_default_set || (new_media == default_media);
  }
  if (!is_default_set && default_media.IsValid())
    media_capabilities.AddDefaultOption(default_media, true);

  // Allow user defined paper sizes to be repeats of existing paper sizes.
  // Do not allow user defined paper sizes to be the default, for now.
  // TODO(thestig): Figure out the default paper policy here.
  for (const auto& paper : semantic_info.user_defined_papers) {
    printer::Media new_media = ConvertPaperToMedia(paper);
    if (!new_media.IsValid())
      continue;

    media_capabilities.AddOption(new_media);
  }
  return media_capabilities;
}

printer::MediaTypeCapability GetMediaTypeCapabilities(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  printer::MediaTypeCapability media_type_capabilities;

  for (const auto& media_type : semantic_info.media_types) {
    printer::MediaType new_media_type(media_type.vendor_id,
                                      media_type.display_name);
    if (!new_media_type.IsValid()) {
      continue;
    }

    if (media_type_capabilities.Contains(new_media_type)) {
      continue;
    }

    media_type_capabilities.AddDefaultOption(
        new_media_type,
        media_type.vendor_id == semantic_info.default_media_type.vendor_id);
  }

  return media_type_capabilities;
}

printer::DpiCapability GetDpiCapabilities(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  printer::DpiCapability dpi_capabilities;
  bool is_default_set = false;

  printer::Dpi default_dpi(semantic_info.default_dpi.width(),
                           semantic_info.default_dpi.height());
  for (const auto& dpi : semantic_info.dpis) {
    printer::Dpi new_dpi(dpi.width(), dpi.height());
    if (!new_dpi.IsValid())
      continue;

    if (dpi_capabilities.Contains(new_dpi))
      continue;

    if (!default_dpi.IsValid())
      default_dpi = new_dpi;
    dpi_capabilities.AddDefaultOption(new_dpi, new_dpi == default_dpi);
    is_default_set = is_default_set || (new_dpi == default_dpi);
  }
  if (!is_default_set && default_dpi.IsValid())
    dpi_capabilities.AddDefaultOption(default_dpi, true);

  return dpi_capabilities;
}

#if BUILDFLAG(IS_CHROMEOS)
printer::VendorCapabilities GetVendorCapabilities(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  printer::VendorCapabilities vendor_capabilities;
  for (const auto& capability : semantic_info.advanced_capabilities) {
    std::string capability_name = capability.display_name.empty()
                                      ? capability.name
                                      : capability.display_name;
    if (capability.values.empty()) {
      vendor_capabilities.AddOption(
          printer::VendorCapability(capability.name, capability_name,
                                    printer::TypedValueVendorCapability(
                                        ToCloudValueType(capability.type))));
      continue;
    }

    printer::SelectVendorCapability select_capability;
    for (const auto& value : capability.values) {
      std::string localized_value =
          value.display_name.empty() ? value.name : value.display_name;
      select_capability.AddDefaultOption(
          printer::SelectVendorCapabilityOption(value.name, localized_value),
          value.name == capability.default_value);
    }
    vendor_capabilities.AddOption(printer::VendorCapability(
        capability.name, capability_name, std::move(select_capability)));
  }

  return vendor_capabilities;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
printer::SelectVendorCapability GetPageOutputQualityCapabilities(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  printer::SelectVendorCapability page_output_quality_capabilities;
  const std::optional<printing::PageOutputQuality>& page_output_quality =
      semantic_info.page_output_quality;
  for (const auto& attribute : page_output_quality->qualities) {
    page_output_quality_capabilities.AddDefaultOption(
        printer::SelectVendorCapabilityOption(attribute.name,
                                              attribute.display_name),
        attribute.name == page_output_quality->default_quality);
  }
  return page_output_quality_capabilities;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

base::Value PrinterSemanticCapsAndDefaultsToCdd(
    const printing::PrinterSemanticCapsAndDefaults& semantic_info) {
  cloud_devices::CloudDeviceDescription description;

  printer::ContentTypesCapability content_types;
  content_types.AddOption("application/pdf");
  content_types.SaveTo(&description);

  if (semantic_info.collate_capable) {
    printer::CollateCapability collate;
    collate.set_default_value(semantic_info.collate_default);
    collate.SaveTo(&description);
  }

  printer::Copies copies_val;
  copies_val.max_value = semantic_info.copies_max;

  printer::CopiesCapability copies_cap;
  copies_cap.set_value(copies_val);
  copies_cap.SaveTo(&description);

  if (semantic_info.duplex_modes.size() > 1) {
    printer::DuplexCapability duplex;
    for (printing::mojom::DuplexMode mode : semantic_info.duplex_modes) {
      duplex.AddDefaultOption(ToCloudDuplexType(mode),
                              semantic_info.duplex_default == mode);
    }
    duplex.SaveTo(&description);
  }

  printer::ColorCapability color;
  if (semantic_info.color_default || semantic_info.color_changeable) {
    printer::Color standard_color(printer::ColorType::STANDARD_COLOR);
    standard_color.vendor_id =
        base::NumberToString(static_cast<int>(semantic_info.color_model));
    color.AddDefaultOption(standard_color, semantic_info.color_default);
  }
  if (!semantic_info.color_default || semantic_info.color_changeable) {
    printer::Color standard_monochrome(printer::ColorType::STANDARD_MONOCHROME);
    standard_monochrome.vendor_id =
        base::NumberToString(static_cast<int>(semantic_info.bw_model));
    color.AddDefaultOption(standard_monochrome, !semantic_info.color_default);
  }
  color.SaveTo(&description);

  if (!semantic_info.papers.empty()) {
    printer::MediaCapability media = GetMediaCapabilities(semantic_info);
    DCHECK(media.IsValid());
    media.SaveTo(&description);
  }

  // Only create this capability if more than one media type is supported.
  if (semantic_info.media_types.size() > 1) {
    printer::MediaTypeCapability media_type =
        GetMediaTypeCapabilities(semantic_info);
    DCHECK(media_type.IsValid());
    media_type.SaveTo(&description);
  }

  if (!semantic_info.dpis.empty()) {
    printer::DpiCapability dpi = GetDpiCapabilities(semantic_info);
    DCHECK(dpi.IsValid());
    dpi.SaveTo(&description);
  }

  printer::OrientationCapability orientation;
  orientation.AddDefaultOption(printer::OrientationType::PORTRAIT, true);
  orientation.AddOption(printer::OrientationType::LANDSCAPE);
  orientation.AddOption(printer::OrientationType::AUTO_ORIENTATION);
  orientation.SaveTo(&description);

#if BUILDFLAG(IS_CHROMEOS)
  printer::PinCapability pin;
  pin.set_value(semantic_info.pin_supported);
  pin.SaveTo(&description);

  if (!semantic_info.advanced_capabilities.empty()) {
    printer::VendorCapabilities vendor_capabilities =
        GetVendorCapabilities(semantic_info);
    vendor_capabilities.SaveTo(&description);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  if (semantic_info.page_output_quality) {
    printer::VendorCapabilities vendor_capabilities;
    vendor_capabilities.AddOption(printer::VendorCapability(
        kIdPageOutputQuality, kDisplayNamePageOutputQuality,
        GetPageOutputQualityCapabilities(semantic_info)));
    vendor_capabilities.SaveTo(&description);
  }
#endif  // BUILDFLAG(IS_WIN)

  return std::move(description).ToValue();
}

}  // namespace cloud_print
