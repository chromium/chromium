// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/force_installed_deprecated_apps_dialog_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

// static
void ForceInstalledDeprecatedAppsDialogView::CreateAndShowDialog(
    extensions::ExtensionId app_id,
    content::WebContents* web_contents) {
  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->SetButtons(ui::DIALOG_BUTTON_OK);
  delegate->SetModalType(ui::MODAL_TYPE_CHILD);
  delegate->SetShowCloseButton(false);
  delegate->SetOwnedByWidget(true);
  delegate->SetTitle(
      l10n_util::GetPluralStringFUTF16(IDS_DEPRECATED_APPS_RENDERER_TITLE, 1));
  delegate->SetContentsView(
      base::WrapUnique<ForceInstalledDeprecatedAppsDialogView>(
          new ForceInstalledDeprecatedAppsDialogView(app_id, web_contents)));
  delegate->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  delegate->set_margins(
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText));
  constrained_window::ShowWebModalDialogViews(delegate.release(), web_contents);
}

ForceInstalledDeprecatedAppsDialogView::ForceInstalledDeprecatedAppsDialogView(
    extensions::ExtensionId app_id,
    content::WebContents* web_contents)
    : app_id_(app_id), web_contents_(web_contents) {
  auto* browser_context = web_contents->GetBrowserContext();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context)
          ->GetInstalledExtension(app_id_);
  SetUseDefaultFillLayout(true);
  auto* info_label = AddChildView(std::make_unique<views::StyledLabel>());

  std::vector<size_t> offsets;
  std::u16string link_text =
      l10n_util::GetStringUTF16(IDS_DEPRECATED_APPS_LEARN_MORE);
  std::u16string info_text =
      l10n_util::GetStringUTF16(IDS_FORCE_INSTALLED_DEPRECATED_APPS_CONTENT);
  std::u16string label_text = l10n_util::FormatString(
      info_text, {base::UTF8ToUTF16(extension->name()), link_text}, &offsets);

  const size_t offset = offsets.back();

  auto link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          [](content::WebContents* web_contents, const ui::Event& event) {
            web_contents->OpenURL(content::OpenURLParams(
                GURL(chrome::kChromeAppsDeprecationLearnMoreURL),
                content::Referrer(),
                ui::DispositionFromEventFlags(
                    event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB),
                ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false));
          },
          web_contents_));
  link_style.disable_line_wrapping = true;
  info_label->SetText(label_text);
  info_label->AddStyleRange(gfx::Range(offset, offset + link_text.length()),
                            link_style);
}

BEGIN_METADATA(ForceInstalledDeprecatedAppsDialogView, views::View)
END_METADATA
