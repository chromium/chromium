// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"

#include <algorithm>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_ui_controller.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher_ui_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_icon_view.h"
#include "chrome/browser/ui/views/autofill/save_update_address_profile_icon_view.h"
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
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_icon_view.h"
#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "chrome/browser/ui/views/webauthn/webauthn_icon_view.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "ui/views/animation/ink_drop.h"
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

  auto add_page_action_icon = [&params, this](PageActionIconType type,
                                              auto icon) {
    icon->SetVisible(false);
    views::InkDrop::Get(icon.get())
        ->SetVisibleOpacity(params.page_action_icon_delegate
                                ->GetPageActionInkDropVisibleOpacity());
    if (params.icon_color)
      icon->SetIconColor(*params.icon_color);
    if (params.font_list)
      icon->SetFontList(*params.font_list);
    auto* icon_ptr = icon.get();
    if (params.button_observer)
      params.button_observer->ObserveButton(icon_ptr);
    this->icon_container_->AddPageActionIcon(std::move(icon));
    this->page_action_icon_views_.emplace(type, icon_ptr);
    return icon_ptr;
  };

  for (PageActionIconType type : params.types_enabled) {
    switch (type) {
      case PageActionIconType::kPaymentsOfferNotification:
        add_page_action_icon(
            type, std::make_unique<autofill::OfferNotificationIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kBookmarkStar:
        add_page_action_icon(type, std::make_unique<StarView>(
                                       params.command_updater, params.browser,
                                       params.icon_label_bubble_delegate,
                                       params.page_action_icon_delegate));
        break;
      case PageActionIconType::kClickToCall:
        add_page_action_icon(
            type, std::make_unique<SharingIconView>(
                      params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate,
                      base::BindRepeating([](content::WebContents* contents) {
                        return static_cast<SharingUiController*>(
                            ClickToCallUiController::GetOrCreateFromWebContents(
                                contents));
                      }),
                      base::BindRepeating(
                          SharingDialogView::GetAsBubbleForClickToCall)));
        break;
      case PageActionIconType::kCookieControls:
        add_page_action_icon(type, std::make_unique<CookieControlsIconView>(
                                       params.icon_label_bubble_delegate,
                                       params.page_action_icon_delegate));
        break;
      case PageActionIconType::kFind:
        add_page_action_icon(
            type, std::make_unique<FindBarIcon>(
                      params.browser, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kIntentPicker:
        add_page_action_icon(
            type, std::make_unique<IntentPickerView>(
                      params.browser, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kLocalCardMigration:
        add_page_action_icon(
            type, std::make_unique<autofill::LocalCardMigrationIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kManagePasswords:
        DCHECK(params.command_updater);
        add_page_action_icon(
            type, std::make_unique<ManagePasswordsIconViews>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kFileSystemAccess:
        add_page_action_icon(type, std::make_unique<FileSystemAccessIconView>(
                                       params.icon_label_bubble_delegate,
                                       params.page_action_icon_delegate));
        break;
      case PageActionIconType::kPwaInstall:
        DCHECK(params.command_updater);
        add_page_action_icon(
            type, std::make_unique<PwaInstallView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate, params.browser));
        break;
      case PageActionIconType::kQRCodeGenerator:
        add_page_action_icon(
            type, std::make_unique<qrcode_generator::QRCodeGeneratorIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kReaderMode:
        DCHECK(params.command_updater);
        add_page_action_icon(
            type, std::make_unique<ReaderModeIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate,
                      params.browser->profile()->GetPrefs()));
        break;
      case PageActionIconType::kSaveAutofillAddress:
        add_page_action_icon(
            type, std::make_unique<autofill::SaveUpdateAddressProfileIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kSaveCard:
        add_page_action_icon(
            type, std::make_unique<autofill::SavePaymentIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kSendTabToSelf:
        add_page_action_icon(
            type, std::make_unique<send_tab_to_self::SendTabToSelfIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kSharedClipboard:
        add_page_action_icon(
            type,
            std::make_unique<SharingIconView>(
                params.icon_label_bubble_delegate,
                params.page_action_icon_delegate,
                base::BindRepeating([](content::WebContents* contents) {
                  return static_cast<SharingUiController*>(
                      SharedClipboardUiController::GetOrCreateFromWebContents(
                          contents));
                }),
                base::BindRepeating(SharingDialogView::GetAsBubble)));
        break;
      case PageActionIconType::kSharingHub:
        add_page_action_icon(
            type, std::make_unique<sharing_hub::SharingHubIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kSmsRemoteFetcher:
        add_page_action_icon(
            type,
            std::make_unique<SharingIconView>(
                params.icon_label_bubble_delegate,
                params.page_action_icon_delegate,
                base::BindRepeating([](content::WebContents* contents) {
                  return static_cast<SharingUiController*>(
                      SmsRemoteFetcherUiController::GetOrCreateFromWebContents(
                          contents));
                }),
                base::BindRepeating(SharingDialogView::GetAsBubble)));
        break;
      case PageActionIconType::kSideSearch:
        DCHECK(params.command_updater);
        add_page_action_icon(
            type, std::make_unique<SideSearchIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate, params.browser));
        break;
      case PageActionIconType::kTranslate:
        DCHECK(params.command_updater);
        add_page_action_icon(
            type, std::make_unique<TranslateIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kWebAuthn:
        DCHECK(base::FeatureList::IsEnabled(features::kWebAuthConditionalUI));
        add_page_action_icon(
            type, std::make_unique<WebAuthnIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kVirtualCardEnroll:
        add_page_action_icon(
            type, std::make_unique<autofill::VirtualCardEnrollIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kVirtualCardManualFallback:
        add_page_action_icon(
            type, std::make_unique<autofill::VirtualCardManualFallbackIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kZoom:
        zoom_icon_ = add_page_action_icon(
            type, std::make_unique<ZoomView>(params.icon_label_bubble_delegate,
                                             params.page_action_icon_delegate));
        break;
    }
  }

  if (params.browser) {
    zoom_observation_.Observe(zoom::ZoomEventManager::GetForBrowserContext(
        params.browser->profile()));
  }
}

PageActionIconView* PageActionIconController::GetIconView(
    PageActionIconType type) {
  auto result = page_action_icon_views_.find(type);
  return result != page_action_icon_views_.end() ? result->second : nullptr;
}

void PageActionIconController::UpdateAll() {
  for (auto icon_item : page_action_icon_views_)
    icon_item.second->Update();
}

bool PageActionIconController::IsAnyIconVisible() const {
  return std::any_of(
      page_action_icon_views_.begin(), page_action_icon_views_.end(),
      [](auto icon_item) { return icon_item.second->GetVisible(); });
}

bool PageActionIconController::ActivateFirstInactiveBubbleForAccessibility() {
  for (auto icon_item : page_action_icon_views_) {
    auto* icon = icon_item.second;
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
  for (auto icon_item : page_action_icon_views_)
    icon_item.second->SetIconColor(icon_color);
}

void PageActionIconController::SetFontList(const gfx::FontList& font_list) {
  for (auto icon_item : page_action_icon_views_)
    icon_item.second->SetFontList(font_list);
}

void PageActionIconController::ZoomChangedForActiveTab(bool can_show_bubble) {
  if (zoom_icon_)
    zoom_icon_->ZoomChangedForActiveTab(can_show_bubble);
}

std::vector<const PageActionIconView*>
PageActionIconController::GetPageActionIconViewsForTesting() const {
  std::vector<const PageActionIconView*> icon_views;
  std::transform(page_action_icon_views_.cbegin(),
                 page_action_icon_views_.cend(), std::back_inserter(icon_views),
                 [](auto const& item) { return item.second; });
  return icon_views;
}

void PageActionIconController::OnDefaultZoomLevelChanged() {
  ZoomChangedForActiveTab(false);
}
