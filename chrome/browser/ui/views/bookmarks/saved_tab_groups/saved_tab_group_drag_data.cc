// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_drag_data.h"

#include <memory>
#include <string>

#include "base/pickle.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/drag_utils.h"

namespace tab_groups {
namespace {

// The MIME type for the clipboard format for SavedTabGroupDragData.
const char kClipboardFormatString[] = "chromium/x-savedtabgroup-entries";

// Paint `button` to an image, then give that to `data` for its drag image.
void AddButtonImageToOSExchangeData(SavedTabGroupButton* button,
                                    const gfx::Point& press_pt,
                                    const ui::ThemeProvider* theme_provider,
                                    ui::OSExchangeData* data) {
  // The button paints itself relative to its parent View's local coordinates.
  // We want it to paint relative to its own local coordinates, so we
  // temporarily move the button to the origin of the parent's local space to
  // make those coordinate spaces agree.
  const gfx::Rect og_bounds = button->bounds();
  gfx::Rect adjusted_bounds = og_bounds;
  adjusted_bounds.Offset(-og_bounds.OffsetFromOrigin());
  // `adjusted_bounds` is in mirrored coordinates (i.e. origin in the top right
  // in RTL). However painting takes place in unmirrored coordinates (i.e.
  // origin in the top left, even in RTL), so to place the button at the origin,
  // we must place it at the unmirrored origin.
  const gfx::Rect unmirrored_bounds =
      button->parent()->GetMirroredRect(adjusted_bounds);
  button->SetBoundsRect(unmirrored_bounds);

  // Take a snapshot of the button.
  SkBitmap bitmap;
  const float raster_scale = ScaleFactorForDragFromWidget(button->GetWidget());
  const SkColor clear_color = SK_ColorTRANSPARENT;
  button->Paint(views::PaintInfo::CreateRootPaintInfo(
      ui::CanvasPainter(&bitmap, button->size(), raster_scale, clear_color,
                        true /* is_pixel_canvas */)
          .context(),
      button->size()));
  const gfx::ImageSkia image =
      gfx::ImageSkia::CreateFromBitmap(bitmap, raster_scale);
  data->provider().SetDragImage(image, press_pt.OffsetFromOrigin());

  button->SetBoundsRect(og_bounds);
}

}  // anonymous namespace

SavedTabGroupDragData::SavedTabGroupDragData(const base::Uuid guid)
    : guid_(guid) {}

// static
const ui::ClipboardFormatType& SavedTabGroupDragData::GetFormatType() {
  static base::NoDestructor<ui::ClipboardFormatType> format(
      ui::ClipboardFormatType::GetType(kClipboardFormatString));

  return *format;
}

// static
std::optional<SavedTabGroupDragData>
SavedTabGroupDragData::ReadFromOSExchangeData(const ui::OSExchangeData* data) {
  if (!data->HasCustomFormat(GetFormatType())) {
    return std::nullopt;
  }

  std::optional<base::Pickle> drag_data_pickle =
      data->GetPickledData(GetFormatType());
  if (!drag_data_pickle.has_value()) {
    return std::nullopt;
  }

  base::PickleIterator data_iterator(drag_data_pickle.value());
  std::string guid_str;
  if (!data_iterator.ReadString(&guid_str)) {
    return std::nullopt;
  }

  base::Uuid guid = base::Uuid::ParseCaseInsensitive(guid_str);
  if (!guid.is_valid()) {
    return std::nullopt;
  }

  return SavedTabGroupDragData(guid);
}

// static
void SavedTabGroupDragData::WriteToOSExchangeData(
    SavedTabGroupButton* button,
    const gfx::Point& press_pt,
    const ui::ThemeProvider* theme_provider,
    ui::OSExchangeData* data) {
  AddButtonImageToOSExchangeData(button, press_pt, theme_provider, data);

  base::Pickle data_pickle;
  data_pickle.WriteString(button->guid().AsLowercaseString());
  data->SetPickledData(GetFormatType(), data_pickle);
}

}  // namespace tab_groups
