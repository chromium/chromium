// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_button_hover_card.h"

#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace {

ui::ImageModel GetIcon(ToolbarActionViewController* action,
                       content::WebContents* web_contents) {
  return ui::ImageModel::FromImageSkia(
      action->GetIcon(web_contents, InstalledExtensionMenuItemView::kIconSize)
          .AsImageSkia());
}

class ExtensionItemFactory
    : public views::BubbleDialogModelHost::CustomViewFactory {
 public:
  ExtensionItemFactory(const std::u16string& name, const ui::ImageModel& icon)
      : name_(std::move(name)), icon_(icon) {}
  ~ExtensionItemFactory() override = default;

  // views::BubbleDialogModelHost::CustomViewFactory:
  std::unique_ptr<views::View> CreateView() override {
    const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
    const gfx::Insets content_insets = provider->GetDialogInsetsForContentType(
        views::DialogContentType::kText, views::DialogContentType::kText);

    return views::Builder<views::FlexLayoutView>()
        .SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
            0, content_insets.left(), 0, content_insets.right())))
        .AddChildren(views::Builder<views::ImageView>().SetImage(icon_),
                     views::Builder<views::Label>().SetText(name_))
        .Build();
  }

  views::BubbleDialogModelHost::FieldType GetFieldType() const override {
    return views::BubbleDialogModelHost::FieldType::kMenuItem;
  }

 private:
  const std::u16string name_;
  const ui::ImageModel icon_;
};

}  // namespace

void ExtensionsRequestAccessButtonHoverCard::ShowBubble(
    content::WebContents* web_contents,
    views::View* anchor_view,
    std::vector<ToolbarActionViewController*> actions) {
  DCHECK(web_contents);
  DCHECK(!actions.empty());
  std::u16string url = url_formatter::IDNToUnicode(
      url_formatter::StripWWW(web_contents->GetLastCommittedURL().host()));

  ui::DialogModel::Builder dialog_builder =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>());
  dialog_builder.OverrideShowCloseButton(false);

  if (actions.size() == 1) {
    dialog_builder.SetIcon(GetIcon(actions[0], web_contents))
        .AddBodyText(ui::DialogModelLabel(l10n_util::GetStringFUTF16(
            IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_TOOLTIP_SINGLE_EXTENSION,
            actions[0]->GetActionName(), url)));
  } else {
    dialog_builder.AddBodyText(ui::DialogModelLabel(l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_TOOLTIP_MULTIPLE_EXTENSIONS,
        url)));
    for (auto* action : actions) {
      dialog_builder.AddCustomField(std::make_unique<ExtensionItemFactory>(
          action->GetActionName(), GetIcon(action, web_contents)));
    }
  }

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);
  views::BubbleDialogDelegate::CreateBubble(std::move(bubble))->Show();
}
