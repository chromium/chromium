// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "components/printing/common/print.mojom.h"
#include "printing/mojom/print.mojom.h"

namespace chromeos {

crosapi::mojom::PrintSettingsPtr CreatePrintSettings(int preview_id) {
  auto print_settings = crosapi::mojom::PrintSettings::New();
  print_settings->preview_id = preview_id;
  print_settings->request_id = 0;
  print_settings->is_first_request = true;
  print_settings->printer_type = printing::mojom::PrinterType::kLocal;
  print_settings->margin_type = printing::mojom::MarginType::kDefaultMargins;
  print_settings->scaling_type = crosapi::mojom::ScalingType::kDefault;
  print_settings->collate = false;
  print_settings->copies = 1;
  print_settings->color = printing::mojom::ColorModel::kColor;
  print_settings->duplex = printing::mojom::DuplexMode::kDefaultValue;
  print_settings->landscape = true;
  print_settings->scale_factor = 1;
  print_settings->rasterize_pdf = false;
  print_settings->pages_per_sheet = 1;
  print_settings->dpi_horizontal = 100;
  print_settings->dpi_vertical = 100;
  print_settings->page_range = std::vector<uint32_t>({0, 1});
  print_settings->header_footer_enabled = false;
  print_settings->should_print_backgrounds = false;
  print_settings->should_print_selection_only = false;

  auto margins_custom = crosapi::mojom::MarginsCustom::New();
  margins_custom->margin_top = 0;
  margins_custom->margin_bottom = 0;
  margins_custom->margin_left = 0;
  margins_custom->margin_right = 0;
  print_settings->margins_custom = std::move(margins_custom);

  auto media_size = crosapi::mojom::MediaSize::New();
  media_size->height_microns = 100;
  media_size->width_microns = 100;
  media_size->imageable_area_bottom_microns = 100;
  media_size->imageable_area_left_microns = 100;
  media_size->imageable_area_right_microns = 100;
  media_size->imageable_area_top_microns = 100;
  print_settings->media_size = std::move(media_size);

  return print_settings;
}

}  // namespace chromeos
