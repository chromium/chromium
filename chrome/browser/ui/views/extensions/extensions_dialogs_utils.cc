// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"

std::unique_ptr<views::BubbleDialogModelHost::CustomView> CreateExtensionItem(
    const std::u16string& name,
    const ui::ImageModel& icon) {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const gfx::Insets content_insets = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText);

  return std::make_unique<views::BubbleDialogModelHost::CustomView>(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
              0, content_insets.left(), 0, content_insets.right())))
          .AddChildren(views::Builder<views::ImageView>().SetImage(icon),
                       views::Builder<views::Label>().SetText(name))
          .Build(),
      views::BubbleDialogModelHost::FieldType::kMenuItem);
}

// TODO(crbug.com/1325171): Use extensions::IconImage instead of getting the
// action's image. The icon displayed should be the "product" icon and not the
// "action" action based on the web contents.
ui::ImageModel GetIcon(ToolbarActionViewController* action,
                       content::WebContents* web_contents) {
  return ui::ImageModel::FromImageSkia(
      action->GetIcon(web_contents, InstalledExtensionMenuItemView::kIconSize)
          .AsImageSkia());
}

std::u16string GetCurrentHost(content::WebContents* web_contents) {
  DCHECK(web_contents);
  auto url = web_contents->GetLastCommittedURL();
  // Hide the scheme when necessary (e.g hide "https://" but don't
  // "chrome://").
  return url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlTrimAfterHost,
      base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
}
