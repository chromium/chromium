// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble_model.h"
#include "chrome/browser/ui/extensions/extension_installed_waiter.h"
#include "chrome/browser/ui/signin/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/promos/bubble_signin_promo_view.h"
#endif

namespace {

const int kRightColumnWidth = 285;
constexpr gfx::Size kMaxIconSize{43, 43};

views::Label* CreateLabel(const std::u16string& text) {
  views::Label* label = new views::Label(text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SizeToFit(kRightColumnWidth);
  return label;
}

views::View* AnchorViewForBrowser(const ExtensionInstalledBubbleModel* model,
                                  Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* reference_view = nullptr;

  if (model->anchor_to_action()) {
    ExtensionsToolbarContainer* const container =
        browser_view->toolbar_button_provider()
            ->GetExtensionsToolbarContainer();
    if (container)
      reference_view = container->GetViewForId(model->extension_id());
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<views::View> CreateSigninPromoView(
    Profile* profile,
    BubbleSignInPromoDelegate* delegate) {
  return std::make_unique<BubbleSignInPromoView>(
      profile, delegate,
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
      IDS_EXTENSION_INSTALLED_DICE_PROMO_SYNC_MESSAGE,
      ui::ButtonStyle::kProminent);
}
#endif

}  // namespace

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the Bubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the pageAction icon in the location
//                      bar which is shown while the Bubble is shown.
//    GENERIC        -> The app menu. This case includes pageActions that don't
//                      specify a default icon.
class ExtensionInstalledBubbleView : public BubbleSignInPromoDelegate,
                                     public views::BubbleDialogDelegateView {
  METADATA_HEADER(ExtensionInstalledBubbleView, views::BubbleDialogDelegateView)

 public:
  ExtensionInstalledBubbleView(
      Browser* browser,
      std::unique_ptr<ExtensionInstalledBubbleModel> model);
  ExtensionInstalledBubbleView(const ExtensionInstalledBubbleView&) = delete;
  ExtensionInstalledBubbleView& operator=(const ExtensionInstalledBubbleView&) =
      delete;
  ~ExtensionInstalledBubbleView() override;

  static void Show(Browser* browser,
                   std::unique_ptr<ExtensionInstalledBubbleModel> model);

  // Recalculate the anchor position for this bubble.
  void UpdateAnchorView();

  const ExtensionInstalledBubbleModel* model() const { return model_.get(); }

 private:
  // views::BubbleDialogDelegateView:
  void Init() override;

  // BubbleSignInPromoDelegate:
  void OnSignIn(const AccountInfo& account_info) override;

  void LinkClicked();

  const raw_ptr<Browser> browser_;
  const std::unique_ptr<ExtensionInstalledBubbleModel> model_;
};

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
  if (model_->show_sign_in_promo()) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    SetFootnoteView(CreateSigninPromoView(browser->profile(), this));
#endif
  }
  SetIcon(ui::ImageModel::FromImageSkia(model_->MakeIconOfSize(kMaxIconSize)));
  SetShowIcon(true);
  SetShowCloseButton(true);

  std::u16string extension_name =
      extensions::util::GetFixupExtensionNameForUIDisplay(
          model_->extension_name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  SetTitle(l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                      extension_name));
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
  layout->set_minimum_cross_axis_size(kRightColumnWidth);
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

  if (model_->show_how_to_use())
    AddChildView(CreateLabel(model_->GetHowToUseText()));

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

void ExtensionInstalledBubbleView::OnSignIn(const AccountInfo& account) {
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      browser_->profile(), account,
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE);
  GetWidget()->Close();
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

void ExtensionInstallUI::ShowBubble(
    scoped_refptr<const extensions::Extension> extension,
    Browser* browser,
    const SkBitmap& icon) {
  ExtensionInstalledWaiter::WaitForInstall(
      extension, browser,
      base::BindOnce(&ShowUiOnToolbarMenu, extension, browser, icon));
}
