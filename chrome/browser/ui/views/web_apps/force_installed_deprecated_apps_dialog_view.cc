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
#include "ui/views/layout/box_layout.h"
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
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  auto* info_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(IDS_FORCE_INSTALLED_DEPRECATED_APPS_CONTENT,
                                 base::UTF8ToUTF16(extension->name()))));
  info_label->SetMultiLine(true);
  info_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* learn_more = AddChildView(std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_DEPRECATED_APPS_LEARN_MORE)));
  learn_more->SetCallback(base::BindRepeating(
      [](content::WebContents* web_contents, const ui::Event& event) {
        web_contents->OpenURL(content::OpenURLParams(
            GURL(chrome::kChromeAppsDeprecationLearnMoreURL),
            content::Referrer(),
            ui::DispositionFromEventFlags(
                event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB),
            ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false));
      },
      web_contents_));
  learn_more->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_FORCE_INSTALLED_DEPRECATED_APPS_LEARN_MORE_AX_LABEL));
  learn_more->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

BEGIN_METADATA(ForceInstalledDeprecatedAppsDialogView, views::View)
END_METADATA
