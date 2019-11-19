// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_ui_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_icon_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls_icon_view.h"
#include "chrome/browser/ui/views/location_bar/find_bar_icon.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_access_icon_view.h"
#include "chrome/browser/ui/views/page_action/pwa_install_view.h"
#include "chrome/browser/ui/views/page_action/zoom_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_icon_view.h"
#include "chrome/browser/ui/views/reader_mode/reader_mode_icon_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_icon_view.h"
#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"
#include "chrome/browser/ui/views/sharing/sharing_icon_view.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "ui/views/layout/box_layout.h"

PageActionIconContainerView::Params::Params() = default;
PageActionIconContainerView::Params::~Params() = default;

// static
const char
    PageActionIconContainerView::kPageActionIconContainerViewClassName[] =
        "PageActionIconContainerView";

PageActionIconContainerView::PageActionIconContainerView(const Params& params)
    : zoom_observer_(this) {
  DCHECK(params.page_action_icon_delegate);

  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          params.between_icon_spacing));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  for (PageActionIconType type : params.types_enabled) {
    switch (type) {
      case PageActionIconType::kBookmarkStar:
        bookmark_star_icon_ =
            new StarView(params.command_updater, params.browser,
                         params.page_action_icon_delegate);
        page_action_icons_.push_back(bookmark_star_icon_);
        break;
      case PageActionIconType::kClickToCall:
        click_to_call_icon_ = new SharingIconView(
            params.page_action_icon_delegate,
            base::BindRepeating([](content::WebContents* contents) {
              return static_cast<SharingUiController*>(
                  ClickToCallUiController::GetOrCreateFromWebContents(
                      contents));
            }),
            base::BindRepeating(SharingDialogView::GetAsBubbleForClickToCall));
        page_action_icons_.push_back(click_to_call_icon_);
        break;
      case PageActionIconType::kCookieControls:
        cookie_controls_icon_ =
            new CookieControlsIconView(params.page_action_icon_delegate);
        page_action_icons_.push_back(cookie_controls_icon_);
        break;
      case PageActionIconType::kFind:
        find_icon_ =
            new FindBarIcon(params.browser, params.page_action_icon_delegate);
        page_action_icons_.push_back(find_icon_);
        break;
      case PageActionIconType::kIntentPicker:
        intent_picker_icon_ = new IntentPickerView(
            params.browser, params.page_action_icon_delegate);
        page_action_icons_.push_back(intent_picker_icon_);
        break;
      case PageActionIconType::kLocalCardMigration:
        local_card_migration_icon_ = new autofill::LocalCardMigrationIconView(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(local_card_migration_icon_);
        break;
      case PageActionIconType::kManagePasswords:
        DCHECK(params.command_updater);
        manage_passwords_icon_ = new ManagePasswordsIconViews(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(manage_passwords_icon_);
        break;
      case PageActionIconType::kNativeFileSystemAccess:
        native_file_system_access_icon_ = new NativeFileSystemAccessIconView(
            params.page_action_icon_delegate);
        page_action_icons_.push_back(native_file_system_access_icon_);
        break;
      case PageActionIconType::kPwaInstall:
        DCHECK(params.command_updater);
        pwa_install_icon_ = new PwaInstallView(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(pwa_install_icon_);
        break;
      case PageActionIconType::kQRCodeGenerator:
        qrcode_generator_icon_view_ =
            new qrcode_generator::QRCodeGeneratorIconView(
                params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(qrcode_generator_icon_view_);
        break;
      case PageActionIconType::kReaderMode:
        DCHECK(params.command_updater);
        reader_mode_icon_ = new ReaderModeIconView(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(reader_mode_icon_);
        break;
      case PageActionIconType::kSaveCard:
        save_card_icon_ = new autofill::SaveCardIconView(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(save_card_icon_);
        break;
      case PageActionIconType::kSendTabToSelf:
        send_tab_to_self_icon_ = new send_tab_to_self::SendTabToSelfIconView(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(send_tab_to_self_icon_);
        break;
      case PageActionIconType::kSharedClipboard:
        shared_clipboard_icon_ = new SharingIconView(
            params.page_action_icon_delegate,
            base::BindRepeating([](content::WebContents* contents) {
              return static_cast<SharingUiController*>(
                  SharedClipboardUiController::GetOrCreateFromWebContents(
                      contents));
            }),
            base::BindRepeating(SharingDialogView::GetAsBubble));
        page_action_icons_.push_back(shared_clipboard_icon_);
        break;
      case PageActionIconType::kTranslate:
        DCHECK(params.command_updater);
        translate_icon_ = new TranslateIconView(
            params.command_updater, params.page_action_icon_delegate);
        page_action_icons_.push_back(translate_icon_);
        break;
      case PageActionIconType::kZoom:
        zoom_icon_ = new ZoomView(params.page_action_icon_delegate);
        page_action_icons_.push_back(zoom_icon_);
        break;
    }
  }

  for (PageActionIconView* icon : page_action_icons_) {
    icon->SetVisible(false);
    icon->set_ink_drop_visible_opacity(
        params.page_action_icon_delegate->GetPageActionInkDropVisibleOpacity());
    if (params.icon_size)
      icon->set_icon_size(*params.icon_size);
    if (params.icon_color)
      icon->SetIconColor(*params.icon_color);
    if (params.font_list)
      icon->SetFontList(*params.font_list);
    if (params.button_observer)
      icon->AddButtonObserver(params.button_observer);
    if (params.view_observer)
      icon->views::View::AddObserver(params.view_observer);
    AddChildView(icon);
  }

  if (params.browser) {
    zoom_observer_.Add(zoom::ZoomEventManager::GetForBrowserContext(
        params.browser->profile()));
  }
}

PageActionIconContainerView::~PageActionIconContainerView() {}

PageActionIconView* PageActionIconContainerView::GetIconView(
    PageActionIconType type) {
  switch (type) {
    case PageActionIconType::kBookmarkStar:
      return bookmark_star_icon_;
    case PageActionIconType::kClickToCall:
      return click_to_call_icon_;
    case PageActionIconType::kCookieControls:
      return cookie_controls_icon_;
    case PageActionIconType::kFind:
      return find_icon_;
    case PageActionIconType::kIntentPicker:
      return intent_picker_icon_;
    case PageActionIconType::kLocalCardMigration:
      return local_card_migration_icon_;
    case PageActionIconType::kManagePasswords:
      return manage_passwords_icon_;
    case PageActionIconType::kNativeFileSystemAccess:
      return native_file_system_access_icon_;
    case PageActionIconType::kPwaInstall:
      return pwa_install_icon_;
    case PageActionIconType::kQRCodeGenerator:
      return qrcode_generator_icon_view_;
    case PageActionIconType::kReaderMode:
      return reader_mode_icon_;
    case PageActionIconType::kSaveCard:
      return save_card_icon_;
    case PageActionIconType::kSendTabToSelf:
      return send_tab_to_self_icon_;
    case PageActionIconType::kSharedClipboard:
      return shared_clipboard_icon_;
    case PageActionIconType::kTranslate:
      return translate_icon_;
    case PageActionIconType::kZoom:
      return zoom_icon_;
  }
  return nullptr;
}

void PageActionIconContainerView::UpdateAll() {
  for (PageActionIconView* icon : page_action_icons_)
    icon->Update();
}

bool PageActionIconContainerView::IsAnyIconVisible() const {
  return std::any_of(
      page_action_icons_.begin(), page_action_icons_.end(),
      [](const PageActionIconView* icon) { return icon->GetVisible(); });
}

bool PageActionIconContainerView::
    ActivateFirstInactiveBubbleForAccessibility() {
  for (PageActionIconView* icon : page_action_icons_) {
    if (!icon->GetVisible() || !icon->GetBubble())
      continue;

    views::Widget* widget = icon->GetBubble()->GetWidget();
    if (widget && widget->IsVisible() && !widget->IsActive()) {
      widget->Show();
      return true;
    }
  }
  return false;
}

void PageActionIconContainerView::SetIconColor(SkColor icon_color) {
  for (PageActionIconView* icon : page_action_icons_)
    icon->SetIconColor(icon_color);
}

void PageActionIconContainerView::SetFontList(const gfx::FontList& font_list) {
  for (PageActionIconView* icon : page_action_icons_)
    icon->SetFontList(font_list);
}

void PageActionIconContainerView::ZoomChangedForActiveTab(
    bool can_show_bubble) {
  if (zoom_icon_)
    zoom_icon_->ZoomChangedForActiveTab(can_show_bubble);
}

const char* PageActionIconContainerView::GetClassName() const {
  return kPageActionIconContainerViewClassName;
}

void PageActionIconContainerView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void PageActionIconContainerView::OnDefaultZoomLevelChanged() {
  ZoomChangedForActiveTab(false);
}
