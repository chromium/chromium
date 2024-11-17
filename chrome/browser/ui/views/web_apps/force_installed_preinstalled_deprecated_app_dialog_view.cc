// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/force_installed_preinstalled_deprecated_app_dialog_view.h"

#include <optional>

#include "ash/constants/web_app_id_constants.h"
#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace {

std::optional<ForceInstalledPreinstalledDeprecatedAppDialogView::LinkConfig>&
GetLinkConfigForTesting() {
  static base::NoDestructor<std::optional<
      ForceInstalledPreinstalledDeprecatedAppDialogView::LinkConfig>>
      g_testing_link_config;
  return *g_testing_link_config;
}

const base::flat_map<
    extensions::ExtensionId,
    ForceInstalledPreinstalledDeprecatedAppDialogView::LinkConfig>&
GetChromeAppConfigs() {
  static base::NoDestructor<base::flat_map<
      extensions::ExtensionId,
      ForceInstalledPreinstalledDeprecatedAppDialogView::LinkConfig>>
      g_chrome_app_configs(
          {{extension_misc::kGmailAppId,
            {.link = GURL("https://mail.google.com/mail/?usp=chrome_app"),
             .link_text = u"mail.google.com",
             .site = ForceInstalledPreinstalledDeprecatedAppDialogView::Site::
                 kGmail}},
           {extension_misc::kGoogleDocsAppId,
            {.link = GURL("https://docs.google.com/document/?usp=chrome_app"),
             .link_text = u"docs.google.com",
             .site = ForceInstalledPreinstalledDeprecatedAppDialogView::Site::
                 kDocs}},
           {extension_misc::kGoogleDriveAppId,
            {.link = GURL("https://drive.google.com/?lfhs=2"),
             .link_text = u"drive.google.com",
             .site = ForceInstalledPreinstalledDeprecatedAppDialogView::Site::
                 kDrive}},
           {extension_misc::kGoogleSheetsAppId,
            {.link =
                 GURL("https://docs.google.com/spreadsheets/?usp=chrome_app"),
             .link_text = u"sheets.google.com",
             .site = ForceInstalledPreinstalledDeprecatedAppDialogView::Site::
                 kSheets}},
           {extension_misc::kGoogleSlidesAppId,
            {.link =
                 GURL("https://docs.google.com/presentation/?usp=chrome_app"),
             .link_text = u"slides.google.com",
             .site = ForceInstalledPreinstalledDeprecatedAppDialogView::Site::
                 kSlides}},
           {extension_misc::kYoutubeAppId,
            {.link = GURL("https://www.youtube.com/?feature=ytca"),
             .link_text = u"www.youtube.com",
             .site = ForceInstalledPreinstalledDeprecatedAppDialogView::Site::
                 kYoutube}}});
  return *g_chrome_app_configs;
}

}  // namespace

// static
void ForceInstalledPreinstalledDeprecatedAppDialogView::CreateAndShowDialog(
    const extensions::ExtensionId& extension_id,
    content::WebContents* web_contents) {
  CHECK(extensions::IsPreinstalledAppId(extension_id));
  auto* browser_context = web_contents->GetBrowserContext();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context)
          ->GetInstalledExtension(extension_id);
  DCHECK(extension);
  std::u16string app_name = base::UTF8ToUTF16(extension->name());
  LinkConfig link_config;
  if (GetLinkConfigForTesting()) {                    // IN-TEST
    link_config = GetLinkConfigForTesting().value();  // IN-TEST
  } else {
    CHECK(base::Contains(GetChromeAppConfigs(), extension_id));
    link_config = GetChromeAppConfigs().at(extension_id);
  }

  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->SetModalType(ui::mojom::ModalType::kChild);
  delegate->SetShowCloseButton(false);
  delegate->SetOwnedByWidget(true);
  delegate->SetTitle(l10n_util::GetStringUTF16(
      IDS_FORCE_INSTALLED_PREINSTALLED_DEPRECATED_APPS_TITLE));
  delegate->SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(
          IDS_FORCE_INSTALLED_PREINSTALLED_DEPRECATED_APPS_GO_TO_SITE_BUTTON));
  delegate->SetAcceptCallback(base::BindOnce(
      [](base::WeakPtr<content::WebContents> web_contents, GURL url,
         Site site) {
        base::UmaHistogramEnumeration(
            "Extensions.ForceInstalledPreInstalledDeprecatedAppOpenUrl", site);
        web_contents->OpenURL(
            content::OpenURLParams(url, content::Referrer(),
                                   WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                   ui::PAGE_TRANSITION_LINK,
                                   /*is_renderer_initiated=*/false),
            /*navigation_handle_callback=*/{});
      },
      web_contents->GetWeakPtr(), link_config.link, link_config.site));
  delegate->SetContentsView(
      base::WrapUnique<ForceInstalledPreinstalledDeprecatedAppDialogView>(
          new ForceInstalledPreinstalledDeprecatedAppDialogView(
              app_name, link_config.link, link_config.link_text,
              web_contents)));
  delegate->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  delegate->set_margins(
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText));
  constrained_window::ShowWebModalDialogViews(delegate.release(), web_contents);
}

// static
base::AutoReset<std::optional<
    ForceInstalledPreinstalledDeprecatedAppDialogView::LinkConfig>>
ForceInstalledPreinstalledDeprecatedAppDialogView::
    SetOverrideLinkConfigForTesting(
        const ForceInstalledPreinstalledDeprecatedAppDialogView::LinkConfig&
            link_config) {
  return base::AutoReset<std::optional<
      ForceInstalledPreinstalledDeprecatedAppDialogView::LinkConfig>>(
      &GetLinkConfigForTesting(), link_config);  // IN-TEST
}

ForceInstalledPreinstalledDeprecatedAppDialogView::
    ForceInstalledPreinstalledDeprecatedAppDialogView(
        const std::u16string& app_name,
        const GURL& app_link,
        const std::u16string& link_string,
        content::WebContents* web_contents) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetInsideBorderInsets(gfx::Insets());
  SetBetweenChildSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL));

  auto* content_label = AddChildView(std::make_unique<views::StyledLabel>());
  content_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  std::vector<size_t> offsets;
  content_label->SetText(l10n_util::GetStringFUTF16(
      IDS_FORCE_INSTALLED_PREINSTALLED_DEPRECATED_APPS_CONTENT, link_string,
      app_name, &offsets));
  DCHECK_EQ(2u, offsets.size());
  content_label->AddStyleRange(
      gfx::Range(offsets[0], offsets[0] + link_string.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          [](base::WeakPtr<content::WebContents> web_contents, GURL url,
             const ui::Event& event) {
            if (!web_contents) {
              return;
            }
            web_contents->OpenURL(
                content::OpenURLParams(
                    url, content::Referrer(),
                    ui::DispositionFromEventFlags(
                        event.flags(),
                        WindowOpenDisposition::NEW_FOREGROUND_TAB),
                    ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false),
                /*navigation_handle_callback=*/{});
          },
          web_contents->GetWeakPtr(), app_link)));

  auto* learn_more = AddChildView(std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_DEPRECATED_APPS_LEARN_MORE)));
  learn_more->SetCallback(base::BindRepeating(
      [](base::WeakPtr<content::WebContents> web_contents,
         const ui::Event& event) {
        if (!web_contents) {
          return;
        }
        web_contents->OpenURL(
            content::OpenURLParams(
                GURL(chrome::kChromeAppsDeprecationLearnMoreURL),
                content::Referrer(),
                ui::DispositionFromEventFlags(
                    event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB),
                ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false),
            /*navigation_handle_callback=*/{});
      },
      web_contents->GetWeakPtr()));
  learn_more->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_DEPRECATED_APPS_LEARN_MORE_AX_LABEL));
  learn_more->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

BEGIN_METADATA(ForceInstalledPreinstalledDeprecatedAppDialogView)
END_METADATA
