// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_button_base.h"

#include <memory>
#include <string_view>

#include "chrome/browser/ui/views/bookmarks/bookmark_button_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/highlight_path_generator.h"

BookmarkMenuButtonBase::BookmarkMenuButtonBase(PressedCallback callback,
                                               std::u16string_view title)
    : MenuButton(std::move(callback), title) {
  ConfigureInkDropForToolbar(this);
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  views::InstallPillHighlightPathGenerator(this);

#if BUILDFLAG(IS_WIN)
  // Paint image(s) to a layer so that the canvas is snapped to pixel
  // boundaries.
  image_container_view()->SetPaintToLayer();
  image_container_view()->layer()->SetFillsBoundsOpaquely(false);
#endif
}

// MenuButton:
std::unique_ptr<views::LabelButtonBorder>
BookmarkMenuButtonBase::CreateDefaultBorder() const {
  return bookmark_button_util::CreateBookmarkButtonBorder();
}

#if BUILDFLAG(IS_WIN)
void BookmarkMenuButtonBase::AddLayerToRegion(ui::Layer* new_layer,
                                              views::LayerRegion region) {
  ink_drop_container()->SetVisible(true);
  ink_drop_container()->AddLayerToRegion(new_layer, region);
}

void BookmarkMenuButtonBase::RemoveLayerFromRegions(ui::Layer* old_layer) {
  ink_drop_container()->RemoveLayerFromRegions(old_layer);
  ink_drop_container()->SetVisible(false);
}
#endif

BEGIN_METADATA(BookmarkMenuButtonBase)
END_METADATA
