// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PUBLIC_MOJOM_PDF_RENDER_SETTINGS_MOJOM_TRAITS_H_
#define CHROME_SERVICES_PRINTING_PUBLIC_MOJOM_PDF_RENDER_SETTINGS_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/services/printing/public/mojom/pdf_render_settings.mojom-shared.h"
#include "printing/pdf_render_settings.h"

namespace mojo {

template <>
struct EnumTraits<printing::mojom::PdfRenderSettings_Mode,
                  printing::PdfRenderSettings::Mode> {
  static printing::mojom::PdfRenderSettings_Mode ToMojom(
      printing::PdfRenderSettings::Mode mode) {
    using MojomMode = printing::mojom::PdfRenderSettings_Mode;
    using PrintMode = printing::PdfRenderSettings::Mode;
    switch (mode) {
      case PrintMode::NORMAL:
        return MojomMode::NORMAL;
#if BUILDFLAG(IS_WIN)
      case PrintMode::TEXTONLY:
        return MojomMode::TEXTONLY;
      case PrintMode::POSTSCRIPT_LEVEL2:
        return MojomMode::POSTSCRIPT_LEVEL2;
      case PrintMode::POSTSCRIPT_LEVEL3:
        return MojomMode::POSTSCRIPT_LEVEL3;
      case PrintMode::EMF_WITH_REDUCED_RASTERIZATION:
        return MojomMode::EMF_WITH_REDUCED_RASTERIZATION;
      case PrintMode::POSTSCRIPT_LEVEL3_WITH_TYPE42_FONTS:
        return MojomMode::POSTSCRIPT_LEVEL3_WITH_TYPE42_FONTS;
#endif
    }
    NOTREACHED_IN_MIGRATION() << "Unknown mode " << static_cast<int>(mode);
    return printing::mojom::PdfRenderSettings_Mode::NORMAL;
  }

  static bool FromMojom(printing::mojom::PdfRenderSettings_Mode input,
                        printing::PdfRenderSettings::Mode* output) {
    using MojomMode = printing::mojom::PdfRenderSettings_Mode;
    using PrintMode = printing::PdfRenderSettings::Mode;
    switch (input) {
      case MojomMode::NORMAL:
        *output = PrintMode::NORMAL;
        return true;
#if BUILDFLAG(IS_WIN)
      case MojomMode::TEXTONLY:
        *output = PrintMode::TEXTONLY;
        return true;
        return true;
      case MojomMode::POSTSCRIPT_LEVEL2:
        *output = PrintMode::POSTSCRIPT_LEVEL2;
        return true;
      case MojomMode::POSTSCRIPT_LEVEL3:
        *output = PrintMode::POSTSCRIPT_LEVEL3;
        return true;
      case MojomMode::EMF_WITH_REDUCED_RASTERIZATION:
        *output = PrintMode::EMF_WITH_REDUCED_RASTERIZATION;
        return true;
      case MojomMode::POSTSCRIPT_LEVEL3_WITH_TYPE42_FONTS:
        *output = PrintMode::POSTSCRIPT_LEVEL3_WITH_TYPE42_FONTS;
        return true;
#endif
    }
    NOTREACHED_IN_MIGRATION() << "Unknown mode " << static_cast<int>(input);
    return false;
  }
};

template <>
class StructTraits<printing::mojom::PdfRenderSettingsDataView,
                   printing::PdfRenderSettings> {
 public:
  static gfx::Rect area(const printing::PdfRenderSettings& settings) {
    return settings.area;
  }
  static gfx::Point offsets(const printing::PdfRenderSettings& settings) {
    return settings.offsets;
  }
  static gfx::Size dpi(const printing::PdfRenderSettings& settings) {
    return settings.dpi;
  }
  static bool autorotate(const printing::PdfRenderSettings& settings) {
    return settings.autorotate;
  }
  static bool use_color(const printing::PdfRenderSettings& settings) {
    return settings.use_color;
  }
  static printing::PdfRenderSettings::Mode mode(
      const printing::PdfRenderSettings& settings) {
    return settings.mode;
  }

  static bool Read(printing::mojom::PdfRenderSettingsDataView data,
                   printing::PdfRenderSettings* out_settings);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_PRINTING_PUBLIC_MOJOM_PDF_RENDER_SETTINGS_MOJOM_TRAITS_H_
