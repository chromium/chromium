// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/mojom/ax_node_data.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"

namespace {
const int kHandleHeight = 24;
const int kHandleWidth = 16;
const int kHandlePadding = 8;
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsResizeHandle,
                                      kMultiContentsResizeHandleElementId);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsResizeArea,
                                      kMultiContentsResizeAreaElementId);

MultiContentsResizeHandle::MultiContentsResizeHandle() {
  SetPreferredSize(gfx::Size(kHandleWidth, kHandleHeight));
  SetCanProcessEventsWithinSubtree(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Install(this);
  SetImage(ui::ImageModel::FromVectorIcon(
      kDragHandleIcon, kColorSidePanelResizeAreaHandle, kHandleWidth));
  GetViewAccessibility().SetRole(ax::mojom::Role::kSlider);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_BY_SIDE_RESIZE));
  SetProperty(views::kElementIdentifierKey,
              kMultiContentsResizeHandleElementId);
}

BEGIN_METADATA(MultiContentsResizeHandle)
END_METADATA

MultiContentsResizeArea::MultiContentsResizeArea(
    MultiContentsView* multi_contents_view)
    : ResizeArea(multi_contents_view),
      multi_contents_view_(multi_contents_view) {
  auto* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  resize_handle_ = AddChildView(std::make_unique<MultiContentsResizeHandle>());

  SetProperty(views::kElementIdentifierKey, kMultiContentsResizeAreaElementId);
  SetPreferredSize(gfx::Size(kHandleWidth + kHandlePadding, kHandleHeight));
}

bool MultiContentsResizeArea::OnKeyPressed(const ui::KeyEvent& event) {
  const int resize_increment = 50;
  if (event.key_code() == ui::VKEY_LEFT) {
    multi_contents_view_->OnResize(-resize_increment, true);
    return true;
  } else if (event.key_code() == ui::VKEY_RIGHT) {
    multi_contents_view_->OnResize(resize_increment, true);
    return true;
  }
  return false;
}

BEGIN_METADATA(MultiContentsResizeArea)
END_METADATA
