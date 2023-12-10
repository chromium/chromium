// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/z_orderable_tab_container_element.h"

#include <bit>
#include <cstdint>

#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/views/view_utils.h"

// static
bool ZOrderableTabContainerElement::CanOrderView(views::View* view) {
  return views::IsViewClass<Tab>(view) ||
         views::IsViewClass<TabGroupHeader>(view) ||
         views::IsViewClass<TabGroupUnderline>(view) ||
         views::IsViewClass<TabGroupHighlight>(view);
}

// static
float ZOrderableTabContainerElement::CalculateZValue(views::View* child) {
  Tab* tab = views::AsViewClass<Tab>(child);
  TabGroupHeader* header = views::AsViewClass<TabGroupHeader>(child);
  TabGroupUnderline* underline = views::AsViewClass<TabGroupUnderline>(child);
  TabGroupHighlight* highlight = views::AsViewClass<TabGroupHighlight>(child);
  DCHECK_EQ(1, !!tab + !!header + !!underline + !!highlight);

  // Construct a bitfield that encodes |child|'s z-value. Higher-order bits
  // encode more important properties - see usage below for details on each.
  // The lowest-order |num_bits_reserved_for_tab_style_z_value| bits are
  // reserved for the factors considered by TabStyle, e.g. selection and hover
  // state.
  constexpr int num_bits_reserved_for_tab_style_z_value =
      std::bit_width<uint32_t>(TabStyle::kMaximumZValue);
  enum ZValue {
    kActiveTab = (1u << (num_bits_reserved_for_tab_style_z_value + 1)),
    kGroupView = (1u << num_bits_reserved_for_tab_style_z_value)
  };

  // Group highlights will keep this z-value and be drawn below everything.
  unsigned int z_value = 0;

  // The active tab is always on top.
  if (tab && tab->IsActive())
    z_value |= kActiveTab;

  // Group headers and underlines are painted above non-active tabs.
  if (header || underline)
    z_value |= kGroupView;

  // The non-active tabs are painted next. They are ordered by their selected
  // or hovered state, which is animated and thus real-valued.
  const float tab_style_z_value =
      tab ? tab->tab_style_views()->GetZValue() + 1.0f : 0.0f;
  return z_value + tab_style_z_value;
}
