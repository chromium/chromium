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
    NOTREACHED_IN_MIGRATION()
        << "Unknown transform type " << static_cast<int>(transform_type);
    return printing::mojom::PwgRasterSettings_TransformType::TRANSFORM_NORMAL;
  }

  static bool FromMojom(printing::mojom::PwgRasterSettings_TransformType input,
                        printing::PwgRasterTransformType* output) {
    switch (input) {
      case printing::mojom::PwgRasterSettings_TransformType::TRANSFORM_NORMAL:
        *output = printing::PwgRasterTransformType::TRANSFORM_NORMAL;
        return true;
      case printing::mojom::PwgRasterSettings_TransformType::
          TRANSFORM_ROTATE_180:
        *output = printing::PwgRasterTransformType::TRANSFORM_ROTATE_180;
        return true;
      case printing::mojom::PwgRasterSettings_TransformType::
          TRANSFORM_FLIP_HORIZONTAL:
        *output = printing::PwgRasterTransformType::TRANSFORM_FLIP_HORIZONTAL;
        return true;
      case printing::mojom::PwgRasterSettings_TransformType::
          TRANSFORM_FLIP_VERTICAL:
        *output = printing::PwgRasterTransformType::TRANSFORM_FLIP_VERTICAL;
        return true;
    }
    NOTREACHED_IN_MIGRATION()
        << "Unknown transform type " << static_cast<int>(input);
    return false;
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
    NOTREACHED_IN_MIGRATION()
        << "Unknown duplex mode " << static_cast<int>(duplex_mode);
    return printing::mojom::PwgRasterSettings_DuplexMode::SIMPLEX;
  }

  static bool FromMojom(printing::mojom::PwgRasterSettings_DuplexMode input,
                        printing::mojom::DuplexMode* output) {
    switch (input) {
      case printing::mojom::PwgRasterSettings_DuplexMode::SIMPLEX:
        *output = printing::mojom::DuplexMode::kSimplex;
        return true;
      case printing::mojom::PwgRasterSettings_DuplexMode::LONG_EDGE:
        *output = printing::mojom::DuplexMode::kLongEdge;
        return true;
      case printing::mojom::PwgRasterSettings_DuplexMode::SHORT_EDGE:
        *output = printing::mojom::DuplexMode::kShortEdge;
        return true;
    }
    NOTREACHED_IN_MIGRATION()
        << "Unknown duplex mode " << static_cast<int>(input);
    return false;
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
