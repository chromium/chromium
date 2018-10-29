// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_installed_bubble_view.h"

#include <algorithm>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bubble/bubble_controller.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

using extensions::Extension;

namespace {

const int kExtensionInstalledIconSize = 43;

const int kRightColumnWidth = 285;

views::Label* CreateLabel(const base::string16& text) {
  views::Label* label = new views::Label(text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SizeToFit(kRightColumnWidth);
  return label;
}

views::View* AnchorViewForBrowser(ExtensionInstalledBubble* controller,
                                  Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* reference_view = nullptr;

  switch (controller->anchor_position()) {
    case ExtensionInstalledBubble::ANCHOR_ACTION: {
      BrowserActionsContainer* container =
          browser_view->toolbar()->browser_actions();
      // Hitting this DCHECK means |ShouldShow| failed.
      DCHECK(container);
      DCHECK(!container->animating());

      reference_view = container->GetViewForId(controller->extension()->id());
      break;
    }
    case ExtensionInstalledBubble::ANCHOR_OMNIBOX: {
      reference_view = browser_view->GetLocationBarView()->location_icon_view();
      break;
    }
    case ExtensionInstalledBubble::ANCHOR_APP_MENU:
      // Will be caught below.
      break;
  }

  // Default case.
  if (!reference_view || !reference_view->visible())
    return browser_view->toolbar_button_provider()->GetAppMenuButton();
  return reference_view;
}

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
class ExtensionInstalledBubbleView : public BubbleSyncPromoDelegate,
                                     public views::BubbleDialogDelegateView,
                                     public views::LinkListener {
 public:
  ExtensionInstalledBubbleView(ExtensionInstalledBubble* bubble,
                               BubbleReference reference);
  ~ExtensionInstalledBubbleView() override;

  // Recalculate the anchor position for this bubble.
  void UpdateAnchorView();

  void CloseBubble(BubbleCloseReason reason);

 private:
  Browser* browser() { return controller_->browser(); }

  // views::BubbleDialogDelegateView:
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;
  View* CreateFootnoteView() override;
  int GetDialogButtons() const override;
  void Init() override;

  // BubbleSyncPromoDelegate:
  void OnEnableSync(const AccountInfo& account_info,
                    bool is_default_promo_account) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // Gets the size of the icon, capped at kExtensionInstalledIconSize.
  gfx::Size GetIconSize() const;

  ExtensionInstalledBubble* controller_;

  BubbleReference bubble_reference_;

  // The shortcut to open the manage shortcuts page.
  views::Link* manage_shortcut_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleView);
};

ExtensionInstalledBubbleView::ExtensionInstalledBubbleView(
    ExtensionInstalledBubble* controller,
    BubbleReference bubble_reference)
    : BubbleDialogDelegateView(nullptr,
                               controller->anchor_position() ==
                                       ExtensionInstalledBubble::ANCHOR_OMNIBOX
                                   ? views::BubbleBorder::TOP_LEFT
                                   : views::BubbleBorder::TOP_RIGHT),
      controller_(controller),
      bubble_reference_(bubble_reference),
      manage_shortcut_(nullptr) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTENSION_INSTALLED);
}

ExtensionInstalledBubbleView::~ExtensionInstalledBubbleView() {}

void ExtensionInstalledBubbleView::UpdateAnchorView() {
  views::View* reference_view = AnchorViewForBrowser(controller_, browser());
  if (reference_view) {
    SetAnchorView(reference_view);
  } else {
    gfx::NativeWindow parent_window = browser()->window()->GetNativeWindow();
    set_parent_window(platform_util::GetViewForWindow(parent_window));
    gfx::Point window_offset = controller_->GetAnchorPoint(parent_window);
    SetAnchorRect(gfx::Rect(window_offset, gfx::Size()));
  }
}

void ExtensionInstalledBubbleView::CloseBubble(BubbleCloseReason reason) {
  // Tells the BubbleController to close the bubble to update the bubble's
  // status in BubbleManager. This does not circulate back to this method
  // because of the nullptr checks in place.
  if (bubble_reference_)
    bubble_reference_->CloseBubble(reason);

  GetWidget()->Close();
}

base::string16 ExtensionInstalledBubbleView::GetWindowTitle() const {
  // Add the heading (for all options).
  base::string16 extension_name =
      base::UTF8ToUTF16(controller_->extension()->name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                    extension_name);
}

gfx::ImageSkia ExtensionInstalledBubbleView::GetWindowIcon() {
  const SkBitmap& bitmap = controller_->icon();
  return gfx::ImageSkiaOperations::CreateResizedImage(
      gfx::ImageSkia::CreateFrom1xBitmap(bitmap),
      skia::ImageOperations::RESIZE_BEST, GetIconSize());
}

bool ExtensionInstalledBubbleView::ShouldShowWindowIcon() const {
  return true;
}

views::View* ExtensionInstalledBubbleView::CreateFootnoteView() {
  if (!(controller_->options() & ExtensionInstalledBubble::SIGN_IN_PROMO))
    return nullptr;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  Profile* profile = browser()->profile();
  if (AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
    return new DiceBubbleSyncPromoView(
        profile, this,
        signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
        IDS_EXTENSION_INSTALLED_DICE_PROMO_SIGNIN_MESSAGE,
        IDS_EXTENSION_INSTALLED_DICE_PROMO_SYNC_MESSAGE);
  } else {
    return new BubbleSyncPromoView(
        this,
        signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
        IDS_EXTENSION_INSTALLED_SYNC_PROMO_LINK_NEW,
        IDS_EXTENSION_INSTALLED_SYNC_PROMO_NEW);
  }
#else
  return new BubbleSyncPromoView(
      this, signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
      IDS_EXTENSION_INSTALLED_SYNC_PROMO_LINK_NEW,
      IDS_EXTENSION_INSTALLED_SYNC_PROMO_NEW);
#endif
}

int ExtensionInstalledBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool ExtensionInstalledBubbleView::ShouldShowCloseButton() const {
  return true;
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
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));
  layout->set_minimum_cross_axis_size(kRightColumnWidth);
  // Indent by the size of the icon.
  layout->set_inside_border_insets(gfx::Insets(
      0,
      GetIconSize().width() +
          provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL),
      0, 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  SetLayoutManager(std::move(layout));

  if (controller_->options() & ExtensionInstalledBubble::HOW_TO_USE)
    AddChildView(CreateLabel(controller_->GetHowToUseDescription()));

  if (controller_->options() & ExtensionInstalledBubble::SHOW_KEYBINDING) {
    manage_shortcut_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS));
    manage_shortcut_->set_listener(this);
    manage_shortcut_->SetUnderline(false);
    AddChildView(manage_shortcut_);
  }

  if (controller_->options() & ExtensionInstalledBubble::HOW_TO_MANAGE) {
    AddChildView(CreateLabel(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO)));
  }
}

void ExtensionInstalledBubbleView::OnEnableSync(const AccountInfo& account,
                                                bool is_default_promo_account) {
  signin_ui_util::EnableSyncFromPromo(
      browser(), account,
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE,
      is_default_promo_account);
  CloseBubble(BUBBLE_CLOSE_NAVIGATED);
}

void ExtensionInstalledBubbleView::LinkClicked(views::Link* source,
                                               int event_flags) {
  DCHECK_EQ(manage_shortcut_, source);

  std::string configure_url = chrome::kChromeUIExtensionsURL;
  configure_url += chrome::kExtensionConfigureCommandsSubPage;
  NavigateParams params(
      GetSingletonTabNavigateParams(browser(), GURL(configure_url)));
  Navigate(&params);
  CloseBubble(BUBBLE_CLOSE_NAVIGATED);
}

gfx::Size ExtensionInstalledBubbleView::GetIconSize() const {
  const SkBitmap& bitmap = controller_->icon();
  // Scale down to 43x43, but allow smaller icons (don't scale up).
  gfx::Size size(bitmap.width(), bitmap.height());
  return size.width() > kExtensionInstalledIconSize ||
                 size.height() > kExtensionInstalledIconSize
             ? gfx::Size(kExtensionInstalledIconSize,
                         kExtensionInstalledIconSize)
             : size;
}

ExtensionInstalledBubbleUi::ExtensionInstalledBubbleUi(
    ExtensionInstalledBubble* bubble)
    : bubble_(bubble), bubble_view_(nullptr) {
  DCHECK(bubble_);
}

ExtensionInstalledBubbleUi::~ExtensionInstalledBubbleUi() {
  if (bubble_view_)
    bubble_view_->GetWidget()->RemoveObserver(this);
}

void ExtensionInstalledBubbleUi::Show(BubbleReference bubble_reference) {
  bubble_view_ = new ExtensionInstalledBubbleView(bubble_, bubble_reference);
  bubble_reference_ = bubble_reference;

  views::BubbleDialogDelegateView::CreateBubble(bubble_view_)->Show();
  bubble_view_->GetWidget()->AddObserver(this);
}

void ExtensionInstalledBubbleUi::Close() {
  if (bubble_view_)
    bubble_view_->CloseBubble(BUBBLE_CLOSE_USER_DISMISSED);
}

void ExtensionInstalledBubbleUi::UpdateAnchorPosition() {
  DCHECK(bubble_view_);
  bubble_view_->UpdateAnchorView();
}

void ExtensionInstalledBubbleUi::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);
  bubble_view_ = nullptr;

  // Tells the BubbleController to close the bubble to update the bubble's
  // status in BubbleManager.
  if (bubble_reference_)
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST);
}

// Views (BrowserView) specific implementation.
bool ExtensionInstalledBubble::ShouldShow() {
  if (anchor_position() == ANCHOR_ACTION) {
    BrowserActionsContainer* container =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->browser_actions();
    return container && !container->animating();
  }
  return true;
}

gfx::Point ExtensionInstalledBubble::GetAnchorPoint(
    gfx::NativeWindow window) const {
  NOTREACHED();  // There is always an anchor view.
  return gfx::Point();
}

std::unique_ptr<BubbleUi> ExtensionInstalledBubble::BuildBubbleUi() {
  return base::WrapUnique(new ExtensionInstalledBubbleUi(this));
}
