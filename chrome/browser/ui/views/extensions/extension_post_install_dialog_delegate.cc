// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_post_install_dialog_delegate.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/sync/account_extension_tracker.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/extensions/extension_install_ui_desktop.h"
#include "chrome/browser/ui/extensions/extension_installed_waiter.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog_model.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/local_data_description.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#endif

namespace {

constexpr gfx::Size kMaxIconSize{43, 43};

#if !BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<views::View> CreateSigninPromoFootnoteView(
    content::WebContents* web_contents,
    const extensions::ExtensionId& extension_id) {
  auto promo_view = std::make_unique<BubbleSignInPromoView>(
      web_contents, signin_metrics::AccessPoint::kExtensionInstallBubble,
      syncer::LocalDataItemModel::DataId(extension_id));

  // Add color and insets to mimic the footnote view on the dialog. We cannot
  // add the footnote using ui::DialogModel because it doesn't support
  // complex footers.
  auto wrapper_view = std::make_unique<views::BoxLayoutView>();
  wrapper_view->SetBackground(
      views::CreateSolidBackground(ui::kColorBubbleFooterBackground));
  const auto& layout_provider = *views::LayoutProvider::Get();
  // Add the dialog insets to the wrapper view to mimic the footnote view.
  // The footnote view adds the dialog insets to its margin, so the promo view
  // also has to add them to have the same visual effect.
  const gfx::Insets dialog_insets =
      layout_provider.GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);
  wrapper_view->SetInsideBorderInsets(
      gfx::Insets::VH(dialog_insets.top(), dialog_insets.left()));

  wrapper_view->AddChildView(std::move(promo_view));
  return wrapper_view;
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace

void ShowExtensionPostInstallDialog(
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<ExtensionPostInstallDialogModel> model) {
  // The Extension Installed bubble is a dialog that informs the user that an
  // extension has been installed. It is constructed using ui::DialogModel,
  // which arranges the elements in a standard dialog format.
  //
  // The dialog consists of:
  // - A title area with the extension's icon and a title indicating the
  //   extension has been installed.
  // - A content area with paragraphs of text, which can include:
  //   - Instructions on how to use the extension.
  //   - A link to configure keyboard shortcuts.
  //   - Information on how to manage the extension.
  // - A footer area that may contain a sign-in or sync promo, added as a
  //   custom view.
  auto delegate = std::make_unique<ExtensionPostInstallDialogDelegate>(
      web_contents, std::move(model));
  gfx::NativeWindow native_window = web_contents->GetTopLevelNativeWindow();
  if (!native_window) {
    return;
  }

  auto* weak_delegate = delegate.get();

  ui::DialogModel::Builder dialog_model_builder(std::move(delegate));

  std::u16string extension_name =
      extensions::util::GetFixupExtensionNameForUIDisplay(
          weak_delegate->model()->extension_name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  dialog_model_builder
      .SetTitle(l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                           extension_name))
      .SetIcon(ui::ImageModel::FromImageSkia(
          weak_delegate->model()->MakeIconOfSize(kMaxIconSize)));

  if (weak_delegate->model()->show_how_to_use()) {
    dialog_model_builder.AddParagraph(
        ui::DialogModelLabel(weak_delegate->model()->GetHowToUseText()));
  }
  if (weak_delegate->model()->show_key_binding()) {
    dialog_model_builder.AddParagraph(
        ui::DialogModelLabel::CreateWithReplacement(
            IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS,
            ui::DialogModelLabel::CreateLink(
                IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS_LINK_TEXT,
                base::BindRepeating(
                    &ExtensionPostInstallDialogDelegate::LinkClicked,
                    base::Unretained(weak_delegate)))));
  }

  if (weak_delegate->model()->show_how_to_manage()) {
    dialog_model_builder.AddParagraph(ui::DialogModelLabel(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO)));
  }
#if !BUILDFLAG(IS_CHROMEOS)
  // Add a sync or sign in promo in the footer if it should be shown.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(
          weak_delegate->model()->extension_id());

  if (signin::ShouldShowExtensionSignInPromo(*profile, *extension) ||
      (signin::ShouldShowExtensionSyncPromo(*profile, *extension) &&
       !switches::IsExtensionsExplicitBrowserSigninEnabled())) {
    // We use a custom field instead of a footnote because footnote doesn't
    // support complex views.
    dialog_model_builder.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            CreateSigninPromoFootnoteView(
                web_contents, weak_delegate->model()->extension_id()),
            views::BubbleDialogModelHost::FieldType::kMenuItem));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  std::unique_ptr<ui::DialogModel> dialog_model = dialog_model_builder.Build();

  // TODO(crbug.com/460843305): Decide how to handle the dialog anchored to
  // omnibox case. Getting the browser view is just temporary as we are moving
  // away from using browser.
  if (weak_delegate->model()->anchor_to_omnibox()) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForNativeWindow(native_window);
    views::View* anchor_view =
        browser_view->GetLocationBarView()->location_icon_view();
    auto bubble = std::make_unique<views::BubbleDialogModelHost>(
        std::move(dialog_model), std::move(anchor_view),
        views::BubbleBorder::TOP_LEFT);
    views::Widget* widget =
        views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
    widget->Show();
  } else {
    ShowDialog(native_window, weak_delegate->model()->extension_id(),
               std::move(dialog_model));
  }
}

ExtensionPostInstallDialogDelegate::ExtensionPostInstallDialogDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<ExtensionPostInstallDialogModel> model)
    : web_contents_(web_contents->GetWeakPtr()), model_(std::move(model)) {}

ExtensionPostInstallDialogDelegate::~ExtensionPostInstallDialogDelegate() =
    default;

void ExtensionPostInstallDialogDelegate::LinkClicked() {
  if (web_contents_) {
    const GURL kUrl(base::StrCat({chrome::kChromeUIExtensionsURL,
                                  chrome::kExtensionConfigureCommandsSubPage}));
    content::OpenURLParams params(
        kUrl, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
    web_contents_->OpenURL(params, {});
  }

  dialog_model()->host()->Close();
}

void ExtensionInstallUIDesktop::ShowBubble(
    scoped_refptr<const extensions::Extension> extension,
    Browser* browser,
    Profile* profile,
    const SkBitmap& icon) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ExtensionInstalledWaiter::WaitForInstall(
      extension, browser,
      base::BindOnce(
          [](Profile* profile, scoped_refptr<const extensions::Extension> ext,
             content::WebContents* contents, const SkBitmap& image) {
            ShowExtensionPostInstallDialog(
                profile, contents,
                std::make_unique<ExtensionPostInstallDialogModel>(
                    profile, ext.get(), image));
          },
          profile, extension, web_contents, icon));
}
