// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PUBLIC_MOJOM_PDF_TO_PWG_RASTER_CONVERTER_MOJOM_TRAITS_H_
#define CHROME_SERVICES_PRINTING_PUBLIC_MOJOM_PDF_TO_PWG_RASTER_CONVERTER_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/services/printing/public/mojom/pdf_to_pwg_raster_converter.mojom-shared.h"
#include "printing/mojom/print.mojom.h"
#include "printing/pwg_raster_settings.h"

namespace mojo {

template <>
struct EnumTraits<printing::mojom::PwgRasterSettings_TransformType,
                  printing::PwgRasterTransformType> {
  static printing::mojom::PwgRasterSettings_TransformType ToMojom(
      printing::PwgRasterTransformType transform_type) {
    switch (transform_type) {
      case printing::PwgRasterTransformType::TRANSFORM_NORMAL:
        return printing::mojom::PwgRasterSettings_TransformType::
            TRANSFORM_NORMAL;
      case printing::PwgRasterTransformType::TRANSFORM_ROTATE_180:
        return printing::mojom::PwgRasterSettings_TransformType::
            TRANSFORM_ROTATE_180;
      case printing::PwgRasterTransformType::TRANSFORM_FLIP_HORIZONTAL:
        return printing::mojom::PwgRasterSettings_TransformType::
            TRANSFORM_FLIP_HORIZONTAL;
      case printing::PwgRasterTransformType::TRANSFORM_FLIP_VERTICAL:
        return printing::mojom::PwgRasterSettings_TransformType::
            TRANSFORM_FLIP_VERTICAL;
    }
    NOTREACHED() << "Unknown transform type "
                 << static_cast<int>(transform_type);
  }

  static printing::PwgRasterTransformType FromMojom(
      printing::mojom::PwgRasterSettings_TransformType input) {
    switch (input) {
      case printing::mojom::PwgRasterSettings_TransformType::TRANSFORM_NORMAL:
        return printing::PwgRasterTransformType::TRANSFORM_NORMAL;
      case printing::mojom::PwgRasterSettings_TransformType::
          TRANSFORM_ROTATE_180:
        return printing::PwgRasterTransformType::TRANSFORM_ROTATE_180;
      case printing::mojom::PwgRasterSettings_TransformType::
          TRANSFORM_FLIP_HORIZONTAL:
        return printing::PwgRasterTransformType::TRANSFORM_FLIP_HORIZONTAL;
      case printing::mojom::PwgRasterSettings_TransformType::
          TRANSFORM_FLIP_VERTICAL:
        return printing::PwgRasterTransformType::TRANSFORM_FLIP_VERTICAL;
    }
    NOTREACHED() << "Unknown transform type " << static_cast<int>(input);
  }
};

template <>
struct EnumTraits<printing::mojom::PwgRasterSettings_DuplexMode,
                  printing::mojom::DuplexMode> {
  static printing::mojom::PwgRasterSettings_DuplexMode ToMojom(
      printing::mojom::DuplexMode duplex_mode) {
    switch (duplex_mode) {
      case printing::mojom::DuplexMode::kUnknownDuplexMode:
        break;
      case printing::mojom::DuplexMode::kSimplex:
        return printing::mojom::PwgRasterSettings_DuplexMode::SIMPLEX;
      case printing::mojom::DuplexMode::kLongEdge:
        return printing::mojom::PwgRasterSettings_DuplexMode::LONG_EDGE;
      case printing::mojom::DuplexMode::kShortEdge:
        return printing::mojom::PwgRasterSettings_DuplexMode::SHORT_EDGE;
    }
    NOTREACHED() << "Unknown duplex mode " << static_cast<int>(duplex_mode);
  }

  static printing::mojom::DuplexMode FromMojom(
      printing::mojom::PwgRasterSettings_DuplexMode input) {
    switch (input) {
      case printing::mojom::PwgRasterSettings_DuplexMode::SIMPLEX:
        return printing::mojom::DuplexMode::kSimplex;
      case printing::mojom::PwgRasterSettings_DuplexMode::LONG_EDGE:
        return printing::mojom::DuplexMode::kLongEdge;
      case printing::mojom::PwgRasterSettings_DuplexMode::SHORT_EDGE:
        return printing::mojom::DuplexMode::kShortEdge;
    }
    NOTREACHED() << "Unknown duplex mode " << static_cast<int>(input);
  }
};

template <>
class StructTraits<printing::mojom::PwgRasterSettingsDataView,
                   printing::PwgRasterSettings> {
 public:
  static bool rotate_all_pages(const printing::PwgRasterSettings& settings) {
    return settings.rotate_all_pages;
  }
  static bool reverse_page_order(const printing::PwgRasterSettings& settings) {
    return settings.reverse_page_order;
  }
  static bool use_color(const printing::PwgRasterSettings& settings) {
    return settings.use_color;
  }
  static printing::PwgRasterTransformType odd_page_transform(
      const printing::PwgRasterSettings& settings) {
    return settings.odd_page_transform;
  }
  static printing::mojom::DuplexMode duplex_mode(
      const printing::PwgRasterSettings& settings) {
    return settings.duplex_mode;
  }

  static bool Read(printing::mojom::PwgRasterSettingsDataView data,
                   printing::PwgRasterSettings* out_settings);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_PRINTING_PUBLIC_MOJOM_PDF_TO_PWG_RASTER_CONVERTER_MOJOM_TRAITS_H_
