// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"

#include <stddef.h>

#include <algorithm>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Solve layout constraints to determine how much space is available for tabs
// to use relative to how much they want to use.
TabSizer CalculateSpaceFractionAvailable(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabWidthConstraints>& tabs,
    std::optional<int> width) {
  if (!width.has_value())
    return TabSizer(LayoutDomain::kInactiveWidthEqualsActiveWidth, 1);

  float minimum_width = 0;
  float crossover_width = 0;
  float preferred_width = 0;
  for (const TabWidthConstraints& tab : tabs) {
    // Add the tab's width, less the width of its trailing foot (which would
    // be double counting).
    minimum_width += tab.GetMinimumWidth() - layout_constants.tab_overlap;
    crossover_width +=
        tab.GetLayoutCrossoverWidth() - layout_constants.tab_overlap;
    preferred_width += tab.GetPreferredWidth() - layout_constants.tab_overlap;
  }

  // Add back the width of the trailing foot of the last tab.
  minimum_width += layout_constants.tab_overlap;
  crossover_width += layout_constants.tab_overlap;
  preferred_width += layout_constants.tab_overlap;

  LayoutDomain domain;
  float space_fraction_available;
  if (width < crossover_width) {
    domain = LayoutDomain::kInactiveWidthBelowActiveWidth;
    // |minimum_width| may equal |crossover_width| when there is only one tab,
    // that tab is active, and the tabstrip width is smaller than that width,
    // which will generally happen during startup of a new window. In this case
    // the layout will always be replaced before we paint, so our return value
    // is irrelevant.
    space_fraction_available = minimum_width == crossover_width
                                   ? 1
                                   : (width.value() - minimum_width) /
                                         (crossover_width - minimum_width);
  } else {
    domain = LayoutDomain::kInactiveWidthEqualsActiveWidth;
    // |preferred_width| may equal |crossover_width| when all tabs are pinned.
    // In this case tabs will have the same width regardless of the space
    // available to them, so our return value is irrelevant.
    space_fraction_available = preferred_width == crossover_width
                                   ? 1
                                   : (width.value() - crossover_width) /
                                         (preferred_width - crossover_width);
  }

  space_fraction_available = std::clamp(space_fraction_available, 0.0f, 1.0f);
  return TabSizer(domain, space_fraction_available);
}

}  // namespace

TabSizer::TabSizer(LayoutDomain domain, float space_fraction_available)
    : domain_(domain), space_fraction_available_(space_fraction_available) {}

int TabSizer::CalculateTabWidth(const TabWidthConstraints& tab) const {
  switch (domain_) {
    case LayoutDomain::kInactiveWidthBelowActiveWidth:
      return std::floor(gfx::Tween::FloatValueBetween(
          space_fraction_available_, tab.GetMinimumWidth(),
          tab.GetLayoutCrossoverWidth()));
    case LayoutDomain::kInactiveWidthEqualsActiveWidth:
      return std::floor(gfx::Tween::FloatValueBetween(
          space_fraction_available_, tab.GetLayoutCrossoverWidth(),
          tab.GetPreferredWidth()));
  }
}

bool TabSizer::TabAcceptsExtraSpace(const TabWidthConstraints& tab) const {
  if (space_fraction_available_ == 0.0f || space_fraction_available_ == 1.0f)
    return false;
  switch (domain_) {
    case LayoutDomain::kInactiveWidthBelowActiveWidth:
      return tab.GetMinimumWidth() < tab.GetLayoutCrossoverWidth();
    case LayoutDomain::kInactiveWidthEqualsActiveWidth:
      return tab.GetLayoutCrossoverWidth() < tab.GetPreferredWidth();
  }
}

bool TabSizer::IsAlreadyPreferredWidth() const {
  return domain_ == LayoutDomain::kInactiveWidthEqualsActiveWidth &&
         space_fraction_available_ == 1;
}

// Because TabSizer::CalculateTabWidth() rounds down, the fractional part of tab
// widths go unused.  Retroactively round up tab widths from left to right to
// use up that width.
void AllocateExtraSpace(std::vector<gfx::Rect>* bounds,
                        const std::vector<TabWidthConstraints>& tabs,
                        std::optional<int> extra_space,
                        TabSizer tab_sizer) {
  // Don't expand tabs if they are already at their preferred width.
  if (tab_sizer.IsAlreadyPreferredWidth() || !extra_space.has_value())
    return;

  int allocated_extra_space = 0;
  for (size_t i = 0; i < tabs.size(); i++) {
    const TabWidthConstraints& tab = tabs[i];
    bounds->at(i).set_x(bounds->at(i).x() + allocated_extra_space);
    if (allocated_extra_space < extra_space &&
        tab_sizer.TabAcceptsExtraSpace(tab)) {
      allocated_extra_space++;
      bounds->at(i).set_width(bounds->at(i).width() + 1);
    }
  }
}

std::vector<gfx::Rect> CalculateTabBounds(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabWidthConstraints>& tabs,
    std::optional<int> width) {
  if (tabs.empty())
    return std::vector<gfx::Rect>();

  TabSizer tab_sizer =
      CalculateSpaceFractionAvailable(layout_constants, tabs, width);

  int next_x = 0;
  std::vector<gfx::Rect> bounds;
  for (const TabWidthConstraints& tab : tabs) {
    const int tab_width = tab_sizer.CalculateTabWidth(tab);
    bounds.emplace_back(next_x, 0, tab_width, layout_constants.tab_height);
    next_x += tab_width - layout_constants.tab_overlap;
  }

  const std::optional<int> calculated_extra_space =
      width.has_value()
          ? std::make_optional(width.value() - bounds.back().right())
          : std::nullopt;
  const std::optional<int> extra_space = calculated_extra_space;
  AllocateExtraSpace(&bounds, tabs, extra_space, tab_sizer);

  return bounds;
}
