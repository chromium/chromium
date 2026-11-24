// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/expandable_site_list_view.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/install_prompt_permissions.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/property_effects.h"

namespace {

std::vector<std::u16string> ParseSitesList(std::u16string_view details) {
  std::vector<std::u16string_view> lines = base::SplitStringPiece(
      details, u"\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::u16string> sites;
  sites.reserve(lines.size());
  for (std::u16string_view line : lines) {
    if (line.starts_with(u"• ")) {
      line.remove_prefix(2);
    }
    sites.emplace_back(line);
  }
  return sites;
}

std::unique_ptr<views::View> CreateSiteItem(const std::u16string& site_text) {
  auto row = std::make_unique<views::View>();
  auto* box = row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  box->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);
  row->GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  row->GetViewAccessibility().SetName(site_text);

  auto* bullet = row->AddChildView(
      views::Builder<views::Label>()
          .SetText(u"• ")
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .Build());
  bullet->GetViewAccessibility().SetIsIgnored(true);

  auto* domain = row->AddChildView(
      views::Builder<views::Label>()
          .SetText(site_text)
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetMultiLine(true)
          .SetAllowCharacterBreak(true)
          .Build());
  domain->GetViewAccessibility().SetIsIgnored(true);
  box->SetFlexForView(domain, 1);
  return row;
}

std::unique_ptr<views::BoxLayoutView> CreateSiteColumn(
    base::span<const std::u16string> sites,
    int child_spacing) {
  auto column = std::make_unique<views::BoxLayoutView>();
  column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  column->SetBetweenChildSpacing(child_spacing);
  column->GetViewAccessibility().SetIsIgnored(true);
  for (const std::u16string& site : sites) {
    column->AddChildView(CreateSiteItem(site));
  }
  return column;
}

}  // namespace

// ExpandableSiteListView::DetailsView -----------------------------------------
ExpandableSiteListView::DetailsView::~DetailsView() = default;

ExpandableSiteListView::DetailsView::DetailsView(
    const std::u16string& visible_details,
    const std::u16string& collapsed_details)
    : visible_sites_(ParseSitesList(visible_details)),
      all_sites_(ParseSitesList(
          collapsed_details.empty()
              ? visible_details
              : base::StrCat({visible_details, u"\n", collapsed_details}))) {
  CHECK(!visible_details.empty());

  auto* layout_provider = ChromeLayoutProvider::Get();
  if (!collapsed_details.empty()) {
    const int bottom_padding = layout_provider->GetDistanceMetric(
        views::DISTANCE_RELATED_CONTROL_VERTICAL);
    SetBorder(
        views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, bottom_padding, 0)));
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kList);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_EXTENSION_PROMPT_SITE_ACCESS_LISTED_SITES_ARIA_LABEL));
  RebuildSiteListColumns();
}

void ExpandableSiteListView::DetailsView::SetExpanded(bool expanded) {
  if (expanded == expanded_) {
    return;
  }
  expanded_ = expanded;
  RebuildSiteListColumns();
  PreferredSizeChanged();
  OnPropertyChanged(&expanded_, views::PropertyEffects::kPaint);
}

bool ExpandableSiteListView::DetailsView::GetExpanded() const {
  return expanded_;
}

std::u16string ExpandableSiteListView::DetailsView::GetSitesText() const {
  return base::JoinString(expanded_ ? all_sites_ : visible_sites_, u"\n");
}

void ExpandableSiteListView::DetailsView::RebuildSiteListColumns() {
  RemoveAllChildViews();

  auto* layout_provider = ChromeLayoutProvider::Get();
  const int child_spacing = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);

  const base::span<const std::u16string> active_sites =
      expanded_ ? all_sites_ : visible_sites_;
  auto* table = SetLayoutManager(std::make_unique<views::TableLayout>());

  if (expanded_ &&
      active_sites.size() >
          extensions::InstallPromptPermissions::kMaxVisibleDetails) {
    table
        ->AddColumn(views::LayoutAlignment::kStretch,
                    views::LayoutAlignment::kStart, 1.0f,
                    views::TableLayout::ColumnSize::kFixed, 0, 0)
        .AddPaddingColumn(views::TableLayout::kFixedSize,
                          layout_provider->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
        .AddColumn(views::LayoutAlignment::kStretch,
                   views::LayoutAlignment::kStart, 1.0f,
                   views::TableLayout::ColumnSize::kFixed, 0, 0)
        .AddRows(1, views::TableLayout::kFixedSize, 0);

    const size_t col1_count = (active_sites.size() + 1) / 2;
    AddChildView(
        CreateSiteColumn(active_sites.first(col1_count), child_spacing));
    AddChildView(
        CreateSiteColumn(active_sites.subspan(col1_count), child_spacing));
    return;
  }

  table
      ->AddColumn(views::LayoutAlignment::kStretch,
                  views::LayoutAlignment::kStart, 1.0f,
                  views::TableLayout::ColumnSize::kFixed, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize, 0);
  AddChildView(CreateSiteColumn(active_sites, child_spacing));
}

BEGIN_METADATA(ExpandableSiteListView, DetailsView)
ADD_PROPERTY_METADATA(bool, Expanded)
END_METADATA

// ExpandableSiteListView ------------------------------------------------------

ExpandableSiteListView::ExpandableSiteListView(
    const std::u16string& visible_details,
    const std::u16string& collapsed_details,
    base::RepeatingClosure on_toggle_callback)
    : on_toggle_callback_(std::move(on_toggle_callback)) {
  CHECK(!visible_details.empty());
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  details_view_ = AddChildView(
      std::make_unique<DetailsView>(visible_details, collapsed_details));

  if (!collapsed_details.empty()) {
    auto details_link = std::make_unique<views::Link>(
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_ALL));
    details_link->SetCallback(base::BindRepeating(
        &ExpandableSiteListView::ToggleDetailLevel, base::Unretained(this)));
    details_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    details_link->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
    details_link->GetViewAccessibility().SetControlIds(
        {details_view_->GetViewAccessibility().GetUniqueId()});
    details_link_ = AddChildView(std::move(details_link));
    UpdateDetailsLinkState();
  }
}

ExpandableSiteListView::~ExpandableSiteListView() = default;

void ExpandableSiteListView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

std::u16string ExpandableSiteListView::GetSitesText() const {
  return details_view_->GetSitesText();
}

bool ExpandableSiteListView::IsExpandedForTesting() const {
  return details_view_->GetExpanded();
}

void ExpandableSiteListView::ToggleDetailLevel() {
  CHECK(details_link_);
  details_view_->SetExpanded(!details_view_->GetExpanded());
  UpdateDetailsLinkState();
  if (on_toggle_callback_) {
    on_toggle_callback_.Run();
  }
}

void ExpandableSiteListView::UpdateDetailsLinkState() {
  CHECK(details_link_);
  const bool expanded = details_view_->GetExpanded();
  details_link_->SetText(l10n_util::GetStringUTF16(
      expanded ? IDS_EXTENSIONS_SHOW_LESS : IDS_EXTENSIONS_SHOW_ALL));
  details_link_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  if (expanded) {
    details_link_->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_LESS));
    details_link_->GetViewAccessibility().SetIsExpanded();
  } else {
    details_link_->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_EXTENSION_PROMPT_SITE_ACCESS_SHOW_ALL_SITES_ARIA_LABEL));
    details_link_->GetViewAccessibility().SetIsCollapsed();
  }
}

BEGIN_METADATA(ExpandableSiteListView)
END_METADATA
