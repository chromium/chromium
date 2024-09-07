// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/force_installed_deprecated_apps_dialog_view.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

// static
void ForceInstalledDeprecatedAppsDialogView::CreateAndShowDialog(
    const extensions::ExtensionId& app_id,
    content::WebContents* web_contents) {
  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->SetModalType(ui::mojom::ModalType::kChild);
  delegate->SetShowCloseButton(false);
  delegate->SetOwnedByWidget(true);
  auto* browser_context = web_contents->GetBrowserContext();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context)
          ->GetInstalledExtension(app_id);
  std::u16string app_name = base::UTF8ToUTF16(extension->name());
  delegate->SetTitle(l10n_util::GetStringFUTF16(
      IDS_DEPRECATED_APPS_RENDERER_TITLE_WITH_APP_NAME, app_name));
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  delegate->SetContentsView(
      base::WrapUnique<ForceInstalledDeprecatedAppsDialogView>(
          new ForceInstalledDeprecatedAppsDialogView(app_name, web_contents)));
  delegate->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  delegate->set_margins(
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText));
  constrained_window::ShowWebModalDialogViews(delegate.release(), web_contents);
}

ForceInstalledDeprecatedAppsDialogView::ForceInstalledDeprecatedAppsDialogView(
    const std::u16string& app_name,
    content::WebContents* web_contents) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetInsideBorderInsets(gfx::Insets());
  SetBetweenChildSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL));

  auto* info_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_FORCE_INSTALLED_DEPRECATED_APPS_CONTENT)));
  info_label->SetMultiLine(true);
  info_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* learn_more = AddChildView(std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_DEPRECATED_APPS_LEARN_MORE)));
  learn_more->SetCallback(base::BindRepeating(
      [](content::WebContents* web_contents, const ui::Event& event) {
        web_contents->OpenURL(
            content::OpenURLParams(
                GURL(chrome::kChromeAppsDeprecationLearnMoreURL),
                content::Referrer(),
                ui::DispositionFromEventFlags(
                    event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB),
                ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false),
            /*navigation_handle_callback=*/{});
      },
      web_contents));
  learn_more->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_DEPRECATED_APPS_LEARN_MORE_AX_LABEL));
  learn_more->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

BEGIN_METADATA(ForceInstalledDeprecatedAppsDialogView)
END_METADATA
