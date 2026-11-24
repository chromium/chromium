// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/expandable_site_list_view.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"

using ExpandableSiteListViewTest = ChromeViewsTestBase;

TEST_F(ExpandableSiteListViewTest, SiteListWithoutCollapsedDetails) {
  std::u16string visible_details = u"• calendar.google.com\n• figma.com";
  auto container = std::make_unique<ExpandableSiteListView>(
      visible_details, /*collapsed_details=*/u"");

  EXPECT_EQ(nullptr, container->details_link_for_testing());
  EXPECT_EQ(u"calendar.google.com\nfigma.com", container->GetSitesText());

  ui::AXNodeData list_node_data;
  container->details_view_for_testing()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&list_node_data);
  EXPECT_EQ(ax::mojom::Role::kList, list_node_data.role);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_SITE_ACCESS_LISTED_SITES_ARIA_LABEL),
      list_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  // Single column with 2 list items whose accessible names exclude bullets.
  ASSERT_EQ(1u, container->details_view_for_testing()->children().size());
  const auto& items =
      container->details_view_for_testing()->children()[0]->children();
  ASSERT_EQ(2u, items.size());

  ui::AXNodeData item_node_data;
  items[0]->GetViewAccessibility().GetAccessibleNodeData(&item_node_data);
  EXPECT_EQ(ax::mojom::Role::kListItem, item_node_data.role);
  EXPECT_EQ(u"calendar.google.com", item_node_data.GetString16Attribute(
                                        ax::mojom::StringAttribute::kName));
}

TEST_F(ExpandableSiteListViewTest, SiteListExpandAndCollapseTwoColumns) {
  std::u16string visible_details = u"• a.com\n• b.com\n• c.com";
  std::u16string collapsed_details = u"• d.com\n• e.com\n• f.com\n• g.com";
  bool toggle_called = false;
  auto container = std::make_unique<ExpandableSiteListView>(
      visible_details, collapsed_details,
      base::BindRepeating([](bool* called) { *called = true; },
                          &toggle_called));

  views::Link* link = container->details_link_for_testing();
  ASSERT_NE(nullptr, link);
  EXPECT_FALSE(container->IsExpandedForTesting());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_ALL),
            link->GetText());
  EXPECT_EQ(u"a.com\nb.com\nc.com", container->GetSitesText());

  ui::AXNodeData link_node_data;
  link->GetViewAccessibility().GetAccessibleNodeData(&link_node_data);
  EXPECT_EQ(ax::mojom::Role::kButton, link_node_data.role);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_SITE_ACCESS_SHOW_ALL_SITES_ARIA_LABEL),
      link_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_TRUE(link_node_data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(link_node_data.HasState(ax::mojom::State::kExpanded));

  // Collapsed state: 1 column with 3 items.
  ASSERT_EQ(1u, container->details_view_for_testing()->children().size());
  EXPECT_EQ(
      3u,
      container->details_view_for_testing()->children()[0]->children().size());

  // Expand: 2 columns with 4 items in column 1 and 3 items in column 2.
  container->ToggleDetailLevelForTest();
  EXPECT_TRUE(toggle_called);
  EXPECT_TRUE(container->IsExpandedForTesting());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_LESS),
            link->GetText());
  EXPECT_EQ(u"a.com\nb.com\nc.com\nd.com\ne.com\nf.com\ng.com",
            container->GetSitesText());
  ASSERT_EQ(2u, container->details_view_for_testing()->children().size());
  EXPECT_EQ(
      4u,
      container->details_view_for_testing()->children()[0]->children().size());
  EXPECT_EQ(
      3u,
      container->details_view_for_testing()->children()[1]->children().size());

  ui::AXNodeData expanded_link_data;
  link->GetViewAccessibility().GetAccessibleNodeData(&expanded_link_data);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSIONS_SHOW_LESS),
            expanded_link_data.GetString16Attribute(
                ax::mojom::StringAttribute::kName));
  EXPECT_TRUE(expanded_link_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(expanded_link_data.HasState(ax::mojom::State::kCollapsed));

  // Collapse back to 1 column of 3 items.
  container->ToggleDetailLevelForTest();
  EXPECT_FALSE(container->IsExpandedForTesting());
  ASSERT_EQ(1u, container->details_view_for_testing()->children().size());
  EXPECT_EQ(
      3u,
      container->details_view_for_testing()->children()[0]->children().size());
}

TEST_F(ExpandableSiteListViewTest, LongSubdomainWildcardWrapsMultiLine) {
  const std::vector<std::u16string> test_sites = {
      l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_SITE_ACCESS_HOST_AND_SUBDOMAIN,
          u"very-long-subdomain-example-domain-name.com"),
      l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_SITE_ACCESS_HOST_AND_SUBDOMAIN,
          u"averyveryveryveryverylongsubdomain.example.com"),
      u"averyveryveryveryverylongsubdomain.example.com",
  };

  for (const std::u16string& site : test_sites) {
    SCOPED_TRACE(site);
    auto container = std::make_unique<ExpandableSiteListView>(
        u"• " + site, /*collapsed_details=*/u"");

    ASSERT_EQ(1u, container->details_view_for_testing()->children().size());
    views::View* row =
        container->details_view_for_testing()->children()[0]->children()[0];
    ASSERT_EQ(2u, row->children().size());
    auto* domain_label = views::AsViewClass<views::Label>(row->children()[1]);
    ASSERT_NE(nullptr, domain_label);
    EXPECT_TRUE(domain_label->GetMultiLine());
    EXPECT_TRUE(domain_label->GetAllowCharacterBreak());

    // Constrain width so the site string wraps onto multiple lines rather than
    // eliding or truncating "All" or the domain.
    const int wide_height = container->GetHeightForWidth(800);
    const int narrow_height = container->GetHeightForWidth(140);
    EXPECT_GT(narrow_height, wide_height);

    container->SetBounds(0, 0, 140, narrow_height);
    views::test::RunScheduledLayout(container.get());
    EXPECT_FALSE(domain_label->IsDisplayTextTruncated());

    const size_t tld_pos = site.find(u".com");
    ASSERT_NE(std::u16string::npos, tld_pos);
    EXPECT_FALSE(
        domain_label->GetSubstringBounds(gfx::Range(tld_pos, tld_pos + 4))
            .empty());
  }
}
