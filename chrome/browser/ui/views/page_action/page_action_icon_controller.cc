// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_ui_controller.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher_ui_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_icon_view.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_icon_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls_icon_view.h"
#include "chrome/browser/ui/views/location_bar/find_bar_icon.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "chrome/browser/ui/views/page_action/pwa_install_view.h"
#include "chrome/browser/ui/views/page_action/zoom_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_icon_view.h"
#include "chrome/browser/ui/views/reader_mode/reader_mode_icon_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_icon_view.h"
#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"
#include "chrome/browser/ui/views/sharing/sharing_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "chrome/browser/ui/views/webauthn/webauthn_icon_view.h"
#include "content/public/common/content_features.h"
#include "ui/views/layout/box_layout.h"

PageActionIconController::PageActionIconController() = default;
PageActionIconController::~PageActionIconController() = default;

void PageActionIconController::Init(const PageActionIconParams& params,
                                    PageActionIconContainer* icon_container) {
  DCHECK(icon_container);
  DCHECK(!icon_container_);
  DCHECK(params.icon_label_bubble_delegate);
  DCHECK(params.page_action_icon_delegate);

  icon_container_ = icon_container;

  for (PageActionIconType type : params.types_enabled) {
    switch (type) {
      case PageActionIconType::kPaymentsOfferNotification:
        offer_notification_icon_ = new autofill::OfferNotificationIconView(
            params.command_updater, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate);
        page_action_icons_.push_back(offer_notification_icon_);
        break;
      case PageActionIconType::kBookmarkStar:
        bookmark_star_icon_ =
            new StarView(params.command_updater, params.browser,
                         params.icon_label_bubble_delegate,
                         params.page_action_icon_delegate);
        page_action_icons_.push_back(bookmark_star_icon_);
        break;
      case PageActionIconType::kClickToCall:
        click_to_call_icon_ = new SharingIconView(
            params.icon_label_bubble_delegate, params.page_action_icon_delegate,
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
            new CookieControlsIconView(params.icon_label_bubble_delegate,
                                       params.page_action_icon_delegate);
        page_action_icons_.push_back(cookie_controls_icon_);
        break;
      case PageActionIconType::kFind:
        find_icon_ =
            new FindBarIcon(params.browser, params.icon_label_bubble_delegate,
                            params.page_action_icon_delegate);
        page_action_icons_.push_back(find_icon_);
        break;
      case PageActionIconType::kIntentPicker:
        intent_picker_icon_ = new IntentPickerView(
            params.browser, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate);
        page_action_icons_.push_back(intent_picker_icon_);
        break;
      case PageActionIconType::kLocalCardMigration:
        local_card_migration_icon_ = new autofill::LocalCardMigrationIconView(
            params.command_updater, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate);
        page_action_icons_.push_back(local_card_migration_icon_);
        break;
      case PageActionIconType::kManagePasswords:
        DCHECK(params.command_updater);
        manage_passwords_icon_ = new ManagePasswordsIconViews(
            params.command_updater, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate);
        page_action_icons_.push_back(manage_passwords_icon_);
        break;
      case PageActionIconType::kFileSystemAccess:
        file_system_access_icon_ =
            new FileSystemAccessIconView(params.icon_label_bubble_delegate,
                                         params.page_action_icon_delegate);
        page_action_icons_.push_back(file_system_access_icon_);
        break;
      case PageActionIconType::kPwaInstall:
        DCHECK(params.command_updater);
        pwa_install_icon_ = new PwaInstallView(
            params.command_updater, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate, params.browser);
        page_action_icons_.push_back(pwa_install_icon_);
        break;
      case PageActionIconType::kQRCodeGenerator:
        qrcode_generator_icon_view_ =
            new qrcode_generator::QRCodeGeneratorIconView(
                params.command_updater, params.icon_label_bubble_delegate,
                params.page_action_icon_delegate);
        page_action_icons_.push_back(qrcode_generator_icon_view_);
        break;
      case PageActionIconType::kReaderMode:
        DCHECK(params.command_updater);
        reader_mode_icon_ = new ReaderModeIconView(
            params.command_updater, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate,
            params.browser->profile()->GetPrefs());
        page_action_icons_.push_back(reader_mode_icon_);
        break;
      case PageActionIconType::kSaveAutofillAddress:
        save_autofill_address_icon_ = new autofill::SaveAddressProfileIconView(
            params.command_updater, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate);
        page_action_icons_.push_back(save_autofill_address_icon_);
        break;
      case PageActionIconType::kSaveCard:
        save_payment_icon_ = new autofill::SavePaymentIconView(
            params.command_updater, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate);
        page_action_icons_.push_back(save_payment_icon_);
        break;
      case PageActionIconType::kSendTabToSelf:
        send_tab_to_self_icon_ = new send_tab_to_self::SendTabToSelfIconView(
            params.command_updater, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate);
        page_action_icons_.push_back(send_tab_to_self_icon_);
        break;
      case PageActionIconType::kSharedClipboard:
        shared_clipboard_icon_ = new SharingIconView(
            params.icon_label_bubble_delegate, params.page_action_icon_delegate,
            base::BindRepeating([](content::WebContents* contents) {
              return static_cast<SharingUiController*>(
                  SharedClipboardUiController::GetOrCreateFromWebContents(
                      contents));
            }),
            base::BindRepeating(SharingDialogView::GetAsBubble));
        page_action_icons_.push_back(shared_clipboard_icon_);
        break;
      case PageActionIconType::kSmsRemoteFetcher:
        sms_remote_fetcher_icon_ = new SharingIconView(
            params.icon_label_bubble_delegate, params.page_action_icon_delegate,
            base::BindRepeating([](content::WebContents* contents) {
              return static_cast<SharingUiController*>(
                  SmsRemoteFetcherUiController::GetOrCreateFromWebContents(
                      contents));
            }),
            base::BindRepeating(SharingDialogView::GetAsBubble));
        page_action_icons_.push_back(sms_remote_fetcher_icon_);
        break;
      case PageActionIconType::kTranslate:
        DCHECK(params.command_updater);
        translate_icon_ = new TranslateIconView(
            params.command_updater, params.icon_label_bubble_delegate,
            params.page_action_icon_delegate);
        page_action_icons_.push_back(translate_icon_);
        break;
      case PageActionIconType::kWebAuthn:
        DCHECK(base::FeatureList::IsEnabled(features::kWebAuthConditionalUI));
        webauthn_icon_ = new WebAuthnIconView(params.command_updater,
                                              params.icon_label_bubble_delegate,
                                              params.page_action_icon_delegate);
        page_action_icons_.push_back(webauthn_icon_);
        break;
      case PageActionIconType::kZoom:
        zoom_icon_ = new ZoomView(params.icon_label_bubble_delegate,
                                  params.page_action_icon_delegate);
        page_action_icons_.push_back(zoom_icon_);
        break;
    }
  }

  for (PageActionIconView* icon : page_action_icons_) {
    icon->SetVisible(false);
    icon->SetInkDropVisibleOpacity(
        params.page_action_icon_delegate->GetPageActionInkDropVisibleOpacity());
    if (params.icon_color)
      icon->SetIconColor(*params.icon_color);
    if (params.font_list)
      icon->SetFontList(*params.font_list);
    if (params.button_observer)
      params.button_observer->ObserveButton(icon);
    icon_container_->AddPageActionIcon(icon);
  }

  if (params.browser) {
    zoom_observation_.Observe(zoom::ZoomEventManager::GetForBrowserContext(
        params.browser->profile()));
  }
}

PageActionIconView* PageActionIconController::GetIconView(
    PageActionIconType type) {
  switch (type) {
    case PageActionIconType::kPaymentsOfferNotification:
      return offer_notification_icon_;
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
    case PageActionIconType::kFileSystemAccess:
      return file_system_access_icon_;
    case PageActionIconType::kPwaInstall:
      return pwa_install_icon_;
    case PageActionIconType::kQRCodeGenerator:
      return qrcode_generator_icon_view_;
    case PageActionIconType::kReaderMode:
      return reader_mode_icon_;
    case PageActionIconType::kSaveAutofillAddress:
      return save_autofill_address_icon_;
    case PageActionIconType::kSaveCard:
      return save_payment_icon_;
    case PageActionIconType::kSendTabToSelf:
      return send_tab_to_self_icon_;
    case PageActionIconType::kSharedClipboard:
      return shared_clipboard_icon_;
    case PageActionIconType::kSmsRemoteFetcher:
      return sms_remote_fetcher_icon_;
    case PageActionIconType::kTranslate:
      return translate_icon_;
    case PageActionIconType::kWebAuthn:
      return webauthn_icon_;
    case PageActionIconType::kZoom:
      return zoom_icon_;
  }
  return nullptr;
}

void PageActionIconController::UpdateAll() {
  for (PageActionIconView* icon : page_action_icons_)
    icon->Update();
}

bool PageActionIconController::IsAnyIconVisible() const {
  return std::any_of(
      page_action_icons_.begin(), page_action_icons_.end(),
      [](const PageActionIconView* icon) { return icon->GetVisible(); });
}

bool PageActionIconController::ActivateFirstInactiveBubbleForAccessibility() {
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

void PageActionIconController::SetIconColor(SkColor icon_color) {
  for (PageActionIconView* icon : page_action_icons_)
    icon->SetIconColor(icon_color);
}

void PageActionIconController::SetFontList(const gfx::FontList& font_list) {
  for (PageActionIconView* icon : page_action_icons_)
    icon->SetFontList(font_list);
}

void PageActionIconController::ZoomChangedForActiveTab(bool can_show_bubble) {
  if (zoom_icon_)
    zoom_icon_->ZoomChangedForActiveTab(can_show_bubble);
}

std::vector<const PageActionIconView*>
PageActionIconController::GetPageActionIconViewsForTesting() const {
  return std::vector<const PageActionIconView*>(page_action_icons_.begin(),
                                                page_action_icons_.end());
}

void PageActionIconController::OnDefaultZoomLevelChanged() {
  ZoomChangedForActiveTab(false);
}
