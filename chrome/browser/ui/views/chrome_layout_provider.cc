// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_layout_provider.h"

#include <algorithm>

#include "base/feature_list.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/shadow_value.h"

namespace {

ChromeLayoutProvider* g_chrome_layout_provider = nullptr;

}  // namespace

ChromeLayoutProvider::ChromeLayoutProvider() {
  DCHECK_EQ(nullptr, g_chrome_layout_provider);
  g_chrome_layout_provider = this;
}

ChromeLayoutProvider::~ChromeLayoutProvider() {
  DCHECK_EQ(this, g_chrome_layout_provider);
  g_chrome_layout_provider = nullptr;
}

// static
ChromeLayoutProvider* ChromeLayoutProvider::Get() {
  // Check to avoid downcasting a base LayoutProvider.
  DCHECK_EQ(g_chrome_layout_provider, views::LayoutProvider::Get());
  return static_cast<ChromeLayoutProvider*>(views::LayoutProvider::Get());
}

// static
std::unique_ptr<views::LayoutProvider>
ChromeLayoutProvider::CreateLayoutProvider() {
  return std::make_unique<ChromeLayoutProvider>();
}

gfx::Insets ChromeLayoutProvider::GetInsetsMetric(int metric) const {
  DCHECK_LT(metric, views::VIEWS_INSETS_MAX);
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  switch (metric) {
    case views::INSETS_DIALOG:
    case views::INSETS_DIALOG_SUBSECTION: {
      return gfx::Insets::VH(20, 20);
    }
    case views::INSETS_DIALOG_FOOTNOTE: {
      return gfx::Insets::TLBR(10, 20, 15, 20);
    }
    case views::INSETS_CHECKBOX_RADIO_BUTTON: {
      gfx::Insets insets = LayoutProvider::GetInsetsMetric(metric);
      // Checkboxes and radio buttons should be aligned flush to the left edge.
      return gfx::Insets::TLBR(insets.top(), 0, insets.bottom(),
                               insets.right());
    }
    case views::INSETS_VECTOR_IMAGE_BUTTON:
      return gfx::Insets(4);
    case views::InsetsMetric::INSETS_LABEL_BUTTON:
      return touch_ui ? gfx::Insets::VH(8, 8)
                      : LayoutProvider::GetInsetsMetric(metric);
    case INSETS_BOOKMARKS_BAR_BUTTON:
      return touch_ui ? gfx::Insets::VH(8, 10) : gfx::Insets(6);
    case INSETS_TOAST:
      return gfx::Insets::VH(0, 16);
    case INSETS_OMNIBOX_PILL_BUTTON:
      if (!touch_ui) {
        return gfx::Insets::VH(4, 8);
      } else {
        return touch_ui ? gfx::Insets::VH(8, 16) : gfx::Insets::VH(5, 12);
      }
    case INSETS_PAGE_INFO_HOVER_BUTTON: {
      const gfx::Insets insets =
          LayoutProvider::GetInsetsMetric(views::INSETS_LABEL_BUTTON);
      const int horizontal_padding = 20;
      // Hover button in page info requires double the height compared to the
      // label button because it behaves like a menu control.
      return gfx::Insets::VH(insets.height(), horizontal_padding);
    }
    default:
      return LayoutProvider::GetInsetsMetric(metric);
  }
}

int ChromeLayoutProvider::GetDistanceMetric(int metric) const {
  DCHECK_GE(metric, views::VIEWS_DISTANCE_START);
  DCHECK_LT(metric, views::VIEWS_DISTANCE_MAX);

  if (metric < views::VIEWS_DISTANCE_END)
    return LayoutProvider::GetDistanceMetric(metric);

  switch (static_cast<ChromeDistanceMetric>(metric)) {
    case DISTANCE_CONTENT_LIST_VERTICAL_SINGLE:
      return 4;
    case DISTANCE_CONTENT_LIST_VERTICAL_MULTI:
      return 8;
    case DISTANCE_CONTROL_LIST_VERTICAL:
      return 12;
    case DISTANCE_EXTENSIONS_MENU_WIDTH:
      return kMediumDialogWidth;
    case DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE:
      return 20;
    case DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SMALL_SIZE:
      return 16;
    case DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE:
      return 28;
    case DISTANCE_EXTENSIONS_MENU_ICON_SPACING:
      return (GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE) -
              GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE)) /
             2;
    case DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN:
      return GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);
    case DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL:
      return 16;
    case DISTANCE_RELATED_CONTROL_VERTICAL_SMALL:
      return 8;
    case DISTANCE_BUTTON_MINIMUM_WIDTH:
      return GetDistanceMetric(views::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH);
    case DISTANCE_RELATED_LABEL_HORIZONTAL_LIST:
      return 8;
    case DISTANCE_SUBSECTION_HORIZONTAL_INDENT:
      return 0;
    case DISTANCE_TOAST_CONTROL_VERTICAL:
      return 8;
    case DISTANCE_TOAST_LABEL_VERTICAL:
      return 12;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE:
      return 16;
    case DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE:
      return 16;
    case DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE:
      return 20;
    case DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH:
      return kMediumDialogWidth;
    case DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH:
      return kLargeDialogWidth;
    case DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL:
      return 24;
    case DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING:
      return 12;
    case DISTANCE_OMNIBOX_TWO_LINE_CELL_VERTICAL_PADDING:
      return 4;
    case DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE:
      return 16;
    case DISTANCE_SIDE_PANEL_HEADER_BUTTON_MINIMUM_SIZE:
      return 20;
    case DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL:
      return 4;
    case DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW:
      return 20;
    case DISTANCE_INFOBAR_HORIZONTAL_ICON_LABEL_PADDING:
      return 16;
    case DISTANCE_INFOBAR_HEIGHT:
      // Spec says height of button should be 36dp, vertical padding on both
      // top and bottom should be 8dp.
      return 36 + 2 * 8;
    case DISTANCE_PERMISSION_PROMPT_HORIZONTAL_ICON_LABEL_PADDING:
      return 8;
    case DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL:
      return 8;
    case DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING:
      return 4;
    case DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING:
      return 16;
    case DISTANCE_TOAST_BUBBLE_HEIGHT:
      return 48;
    case DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON:
      return 36;
    case DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT:
      return 24;
    case DISTANCE_TOAST_BUBBLE_ICON_SIZE:
      return 20;
    case DISTANCE_TOAST_BUBBLE_LEADING_ICON_SIDE_MARGINS:
      return 2;
    case DISTANCE_TOAST_BUBBLE_MARGIN_LEFT:
      return 12;
    case DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_ACTION_BUTTON:
      return 6;
    case DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON:
      return 12;
    case DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_LABEL:
      return 16;
  }
  NOTREACHED();
}

int ChromeLayoutProvider::GetSnappedDialogWidth(int min_width) const {
  for (int snap_point :
       {kSmallDialogWidth, kMediumDialogWidth, kLargeDialogWidth}) {
    if (min_width <= snap_point)
      return snap_point;
  }

  return ((min_width + 15) / 16) * 16;
}

const views::TypographyProvider& ChromeLayoutProvider::GetTypographyProvider()
    const {
  return typography_provider_;
}

bool ChromeLayoutProvider::ShouldShowWindowIcon() const {
  return false;
}
