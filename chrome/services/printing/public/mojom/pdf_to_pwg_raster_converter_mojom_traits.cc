// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/public/mojom/pdf_to_pwg_raster_converter_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<printing::mojom::PwgRasterSettingsDataView,
                  printing::PwgRasterSettings>::
    Read(printing::mojom::PwgRasterSettingsDataView data,
         printing::PwgRasterSettings* out) {
  out->rotate_all_pages = data.rotate_all_pages();
  out->reverse_page_order = data.reverse_page_order();
  out->use_color = data.use_color();
  return data.ReadOddPageTransform(&out->odd_page_transform) &&
         data.ReadDuplexMode(&out->duplex_mode);
}

}  // namespace mojo
