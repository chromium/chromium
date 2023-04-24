// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_layout_provider.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/shadow_value.h"

namespace {

// TODO(pbos): Inline kHarmonyLayoutUnit calculations below as it's not really
// respected (there's 3 * unit / 4 in use to express 12).
// The Harmony layout unit. All distances are in terms of this unit.
constexpr int kHarmonyLayoutUnit = 16;

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
      return features::IsChromeRefresh2023() ? gfx::Insets::VH(20, 20)
                                             : gfx::Insets(kHarmonyLayoutUnit);
    }
    case views::INSETS_CHECKBOX_RADIO_BUTTON: {
      gfx::Insets insets = LayoutProvider::GetInsetsMetric(metric);
      // Checkboxes and radio buttons should be aligned flush to the left edge.
      return gfx::Insets::TLBR(insets.top(), 0, insets.bottom(),
                               insets.right());
    }
    case views::INSETS_VECTOR_IMAGE_BUTTON:
      return gfx::Insets(kHarmonyLayoutUnit / 4);
    case views::InsetsMetric::INSETS_LABEL_BUTTON:
      return touch_ui ? gfx::Insets::VH(kHarmonyLayoutUnit / 2,
                                        kHarmonyLayoutUnit / 2)
                      : LayoutProvider::GetInsetsMetric(metric);
    case INSETS_BOOKMARKS_BAR_BUTTON:
      return touch_ui ? gfx::Insets::VH(8, 10) : gfx::Insets(6);
    case INSETS_TOAST:
      return gfx::Insets::VH(0, kHarmonyLayoutUnit);
    case INSETS_OMNIBOX_PILL_BUTTON:
      if ((base::FeatureList::IsEnabled(omnibox::kCr2023ActionChips) ||
           features::GetChromeRefresh2023Level() ==
               features::ChromeRefresh2023Level::kLevel2) &&
          !touch_ui) {
        return gfx::Insets::VH(4, 8);
      } else {
        return touch_ui
                   ? gfx::Insets::VH(kHarmonyLayoutUnit / 2, kHarmonyLayoutUnit)
                   : gfx::Insets::VH(5, 12);
      }
    case INSETS_PAGE_INFO_HOVER_BUTTON: {
      const gfx::Insets insets =
          LayoutProvider::GetInsetsMetric(views::INSETS_LABEL_BUTTON);
      const int horizontal_padding =
          GetDistanceMetric(views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
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
      return kHarmonyLayoutUnit / 4;
    case DISTANCE_CONTENT_LIST_VERTICAL_MULTI:
      return kHarmonyLayoutUnit / 2;
    case DISTANCE_CONTROL_LIST_VERTICAL:
      return kHarmonyLayoutUnit * 3 / 4;
    case DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING:
      return 8;
    case DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN:
      return 12;
    case DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE:
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
      return kHarmonyLayoutUnit;
    case DISTANCE_RELATED_CONTROL_VERTICAL_SMALL:
      return kHarmonyLayoutUnit / 2;
    case DISTANCE_BUTTON_MINIMUM_WIDTH:
      return GetDistanceMetric(views::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH);
    case DISTANCE_RELATED_LABEL_HORIZONTAL_LIST:
      return kHarmonyLayoutUnit / 2;
    case DISTANCE_SUBSECTION_HORIZONTAL_INDENT:
      return 0;
    case DISTANCE_TOAST_CONTROL_VERTICAL:
      return 8;
    case DISTANCE_TOAST_LABEL_VERTICAL:
      return 12;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL:
      return kHarmonyLayoutUnit;
    case DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE:
      return kHarmonyLayoutUnit;
    case DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE:
      return kHarmonyLayoutUnit;
    case DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE:
      return 20;
    case DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH:
      return kMediumDialogWidth;
    case DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH:
      return kLargeDialogWidth;
    case DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL:
      return 24;
    case DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING:
      return 8;
    case DISTANCE_OMNIBOX_TWO_LINE_CELL_VERTICAL_PADDING:
      return 4;
    case DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE:
      return 16;
    case DISTANCE_SIDE_PANEL_HEADER_BUTTON_MINIMUM_SIZE:
      return 20;
    case DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL:
      return 8;
  }
  NOTREACHED_NORETURN();
}

int ChromeLayoutProvider::GetSnappedDialogWidth(int min_width) const {
  for (int snap_point :
       {kSmallDialogWidth, kMediumDialogWidth, kLargeDialogWidth}) {
    if (min_width <= snap_point)
      return snap_point;
  }

  return ((min_width + kHarmonyLayoutUnit - 1) / kHarmonyLayoutUnit) *
         kHarmonyLayoutUnit;
}

const views::TypographyProvider& ChromeLayoutProvider::GetTypographyProvider()
    const {
  return typography_provider_;
}

bool ChromeLayoutProvider::ShouldShowWindowIcon() const {
  return false;
}
