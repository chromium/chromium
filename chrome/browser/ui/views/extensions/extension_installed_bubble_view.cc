// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_installed_bubble_view.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_install_ui_desktop.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble_model.h"
#include "chrome/browser/ui/extensions/extension_installed_waiter.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/local_data_description.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#endif

namespace {

const int kRightColumnWidth = 285;

// When used, the entire bubble will be approximately 448px in width.
const int kExplicitSigninRightColumnWidth = 348;
constexpr gfx::Size kMaxIconSize{43, 43};

int GetRightColumnWidth() {
  return switches::IsExtensionsExplicitBrowserSigninEnabled()
             ? kExplicitSigninRightColumnWidth
             : kRightColumnWidth;
}

std::unique_ptr<views::Label> CreateLabel(const std::u16string& text) {
  return views::Builder<views::Label>()
      .SetText(text)
      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
      .SetMultiLine(true)
      .SizeToFit(GetRightColumnWidth())
      .Build();
}

views::View* AnchorViewForBrowser(const ExtensionInstalledBubbleModel* model,
                                  Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* reference_view = nullptr;

  if (model->anchor_to_action()) {
    ExtensionsToolbarContainer* const container =
        browser_view->toolbar_button_provider()
            ->GetExtensionsToolbarContainer();
    if (container) {
      reference_view = container->GetViewForId(model->extension_id());
    }
  } else if (model->anchor_to_omnibox()) {
    reference_view = browser_view->GetLocationBarView()->location_icon_view();
  }

  // Default case.
  if (!reference_view || !reference_view->GetVisible()) {
    return browser_view->toolbar_button_provider()
        ->GetDefaultExtensionDialogAnchorView();
  }
  return reference_view;
}

#if !BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<views::View> CreateSigninPromoView(
    content::WebContents* web_contents,
    const extensions::ExtensionId& extension_id) {
  return std::make_unique<BubbleSignInPromoView>(
      web_contents, signin_metrics::AccessPoint::kExtensionInstallBubble,
      syncer::LocalDataItemModel::DataId(extension_id));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace

// static
void ExtensionInstalledBubbleView::Show(
    Browser* browser,
    std::unique_ptr<ExtensionInstalledBubbleModel> model) {
  auto delegate =
      std::make_unique<ExtensionInstalledBubbleView>(browser, std::move(model));
  auto* weak_delegate = delegate.get();
  views::Widget* const widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(delegate));
  // When the extension is installed to the ExtensionsToolbarContainer, use the
  // container to pop out the extension icon and show the widget.
  if (weak_delegate->model()->anchor_to_action()) {
    ExtensionsToolbarContainer* const container =
        BrowserView::GetBrowserViewForBrowser(browser)
            ->toolbar_button_provider()
            ->GetExtensionsToolbarContainer();
    container->ShowWidgetForExtension(widget,
                                      weak_delegate->model()->extension_id());
  } else {
    widget->Show();
  }
}

ExtensionInstalledBubbleView::ExtensionInstalledBubbleView(
    Browser* browser,
    std::unique_ptr<ExtensionInstalledBubbleModel> model)
    : BubbleDialogDelegateView(AnchorViewForBrowser(model.get(), browser),
                               model->anchor_to_omnibox()
                                   ? views::BubbleBorder::TOP_LEFT
                                   : views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      model_(std::move(model)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetIcon(ui::ImageModel::FromImageSkia(model_->MakeIconOfSize(kMaxIconSize)));
  SetShowIcon(true);
  SetShowCloseButton(true);

  std::u16string extension_name =
      extensions::util::GetFixupExtensionNameForUIDisplay(
          model_->extension_name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  SetTitle(l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                      extension_name));

#if !BUILDFLAG(IS_CHROMEOS)
  // Add a sync or sign in promo in the footer if it should be shown.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser->profile());
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(model_->extension_id());

  if (signin::ShouldShowExtensionSignInPromo(*browser->profile(), *extension) ||
      (signin::ShouldShowExtensionSyncPromo(*browser->profile(), *extension) &&
       !base::FeatureList::IsEnabled(
           switches::kEnableExtensionsExplicitBrowserSignin))) {
    SetFootnoteView(CreateSigninPromoView(
        browser->tab_strip_model()->GetActiveWebContents(),
        model_->extension_id()));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

ExtensionInstalledBubbleView::~ExtensionInstalledBubbleView() = default;

void ExtensionInstalledBubbleView::UpdateAnchorView() {
  views::View* reference_view = AnchorViewForBrowser(model_.get(), browser_);
  DCHECK(reference_view);
  SetAnchorView(reference_view);
}

void ExtensionInstalledBubbleView::Init() {
  UpdateAnchorView();

  // The Extension Installed bubble takes on various forms, depending on the
  // type of extension installed. In general, though, they are all similar:
  //
  // -------------------------
  // | Icon | Title      (x) |
  // |        Info           |
  // |        Extra info     |
  // -------------------------
  //
  // Icon and Title are always shown (as well as the close button).
  // Info is shown for browser actions, page actions and Omnibox keyword
  // extensions and might list keyboard shorcut for the former two types.
  // Extra info is...
  // ... for other types, either a description of how to manage the extension
  //     or a link to configure the keybinding shortcut (if one exists).
  // Extra info can include a promo for signing into sync.

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));
  layout->set_minimum_cross_axis_size(GetRightColumnWidth());
  // Indent by the size of the icon.
  layout->set_inside_border_insets(
      gfx::Insets::TLBR(0,
                        GetWindowIcon().Size().width() +
                            provider->GetDistanceMetric(
                                views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL),
                        0, 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  SetLayoutManager(std::move(layout));

  if (model_->show_how_to_use()) {
    AddChildView(CreateLabel(model_->GetHowToUseText()));
  }

  if (model_->show_key_binding()) {
    auto* manage_shortcut = AddChildView(std::make_unique<views::Link>(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS)));
    manage_shortcut->SetCallback(base::BindRepeating(
        &ExtensionInstalledBubbleView::LinkClicked, base::Unretained(this)));
  }

  if (model_->show_how_to_manage()) {
    AddChildView(CreateLabel(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO)));
  }
}

void ExtensionInstalledBubbleView::LinkClicked() {
  const GURL kUrl(base::StrCat({chrome::kChromeUIExtensionsURL,
                                chrome::kExtensionConfigureCommandsSubPage}));
  NavigateParams params = GetSingletonTabNavigateParams(browser_, kUrl);
  Navigate(&params);
  GetWidget()->Close();
}

BEGIN_METADATA(ExtensionInstalledBubbleView)
END_METADATA

void ShowUiOnToolbarMenu(scoped_refptr<const extensions::Extension> extension,
                         Browser* browser,
                         const SkBitmap& icon) {
  ExtensionInstalledBubbleView::Show(
      browser, std::make_unique<ExtensionInstalledBubbleModel>(
                   browser->profile(), extension.get(), icon));
}

void ExtensionInstallUIDesktop::ShowBubble(
    scoped_refptr<const extensions::Extension> extension,
    Browser* browser,
    const SkBitmap& icon) {
  ExtensionInstalledWaiter::WaitForInstall(
      extension, browser,
      base::BindOnce(&ShowUiOnToolbarMenu, extension, browser, icon));
}
