// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/immediate_crash.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher_ui_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/autofill/address_bubbles_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_icon_view.h"
#include "chrome/browser/ui/views/commerce/discounts_icon_view.h"
#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"
#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"
#include "chrome/browser/ui/views/commerce/product_specifications_icon_view.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_icon_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/views/location_bar/find_bar_icon.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_page_action_icon_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_params.h"
#include "chrome/browser/ui/views/page_action/pwa_install_view.h"
#include "chrome/browser/ui/views/page_action/zoom_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_chip_view.h"
#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"
#include "chrome/browser/ui/views/sharing/sharing_icon_view.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_icon_view.h"
#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/features.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_features.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/layout/box_layout.h"

namespace {

void RecordCTRMetrics(const char* name, PageActionCTREvent event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"PageActionController.", name, ".Icon.CTR2"}), event);
}

}  // namespace

PageActionIconController::PageActionIconController() = default;
PageActionIconController::~PageActionIconController() = default;

void PageActionIconController::Init(const PageActionIconParams& params,
                                    PageActionIconContainer* icon_container) {
  DCHECK(icon_container);
  DCHECK(!icon_container_);
  DCHECK(params.icon_label_bubble_delegate);
  DCHECK(params.page_action_icon_delegate);

  browser_ = params.browser;
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
    icon->AddPageIconViewObserver(this);
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
        add_page_action_icon(
            type, std::make_unique<CookieControlsIconView>(
                      params.browser, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kDiscounts:
        add_page_action_icon(type, std::make_unique<DiscountsIconView>(
                                       params.icon_label_bubble_delegate,
                                       params.page_action_icon_delegate));
        break;
      case PageActionIconType::kFind:
        add_page_action_icon(
            type, std::make_unique<FindBarIcon>(
                      params.browser, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kMemorySaver:
        add_page_action_icon(type, std::make_unique<MemorySaverChipView>(
                                       params.command_updater, params.browser,
                                       params.icon_label_bubble_delegate,
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
                      params.page_action_icon_delegate, params.browser));
        break;
      case PageActionIconType::kMandatoryReauth:
        add_page_action_icon(
            type, std::make_unique<autofill::MandatoryReauthIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kFileSystemAccess:
        add_page_action_icon(type, std::make_unique<FileSystemAccessIconView>(
                                       params.icon_label_bubble_delegate,
                                       params.page_action_icon_delegate));
        break;
      case PageActionIconType::kPriceInsights:
        add_page_action_icon(type, std::make_unique<PriceInsightsIconView>(
                                       params.icon_label_bubble_delegate,
                                       params.page_action_icon_delegate,
                                       params.browser->profile()));
        break;
      case PageActionIconType::kPriceTracking:
        add_page_action_icon(
            type, std::make_unique<PriceTrackingIconView>(
                      params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate, params.browser));
        break;
      case PageActionIconType::kProductSpecifications:
        add_page_action_icon(
            type, std::make_unique<ProductSpecificationsIconView>(
                      params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate, params.browser));
        break;
      case PageActionIconType::kPwaInstall:
        DCHECK(params.command_updater);
        add_page_action_icon(
            type, std::make_unique<PwaInstallView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate, params.browser));
        break;
      case PageActionIconType::kAutofillAddress:
        add_page_action_icon(
            type, std::make_unique<autofill::AddressBubblesIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
      case PageActionIconType::kSaveCard:
        add_page_action_icon(
            type, std::make_unique<autofill::SavePaymentIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate,
                      IDC_SAVE_CREDIT_CARD_FOR_PAGE));
        break;
      case PageActionIconType::kSaveIban:
        add_page_action_icon(
            type,
            std::make_unique<autofill::SavePaymentIconView>(
                params.command_updater, params.icon_label_bubble_delegate,
                params.page_action_icon_delegate, IDC_SAVE_IBAN_FOR_PAGE));
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
        add_page_action_icon(
            type, std::make_unique<SideSearchIconView>(
                      params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate, params.browser));
        break;
      case PageActionIconType::kTranslate:
        DCHECK(params.command_updater);
        add_page_action_icon(
            type, std::make_unique<TranslateIconView>(
                      params.command_updater, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate, params.browser));
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
      case PageActionIconType::kLensOverlay:
        add_page_action_icon(
            type, std::make_unique<LensOverlayPageActionIconView>(
                      params.browser, params.icon_label_bubble_delegate,
                      params.page_action_icon_delegate));
        break;
    }
  }

  if (params.browser) {
    zoom_observation_.Observe(zoom::ZoomEventManager::GetForBrowserContext(
        params.browser->profile()));

    pref_change_registrar_.Init(params.browser->profile()->GetPrefs());
    pref_change_registrar_.Add(
        omnibox::kShowGoogleLensShortcut,
        base::BindRepeating(&PageActionIconController::UpdateAll,
                            base::Unretained(this)));
  }
}

PageActionIconView* PageActionIconController::GetIconView(
    PageActionIconType type) {
  auto result = page_action_icon_views_.find(type);
  return result != page_action_icon_views_.end() ? result->second : nullptr;
}

PageActionIconType PageActionIconController::GetIconType(
    PageActionIconView* view) {
  for (auto& page_action : page_action_icon_views_) {
    if (page_action.second == view) {
      return page_action.first;
    }
  }
  base::ImmediateCrash();
}

void PageActionIconController::UpdateAll() {
  for (auto icon_item : page_action_icon_views_)
    icon_item.second->Update();
  if (!browser_ || !browser_->tab_strip_model() ||
      !browser_->tab_strip_model()->GetActiveWebContents()) {
    return;
  }
  const GURL url =
      browser_->tab_strip_model()->GetActiveWebContents()->GetURL();
  if (page_actions_excluded_from_logging_.find(url) ==
      page_actions_excluded_from_logging_.end()) {
    RecordMetricsOnURLChange(url);
  }
}

bool PageActionIconController::IsAnyIconVisible() const {
  return base::ranges::any_of(page_action_icon_views_, [](auto icon_item) {
    return icon_item.second->GetVisible();
  });
}

bool PageActionIconController::ActivateFirstInactiveBubbleForAccessibility() {
  for (auto icon_item : page_action_icon_views_) {
    auto* icon = icon_item.second.get();
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

void PageActionIconController::OnPageActionIconViewShown(
    PageActionIconView* view) {
  if (!browser_ || !browser_->tab_strip_model() ||
      !browser_->tab_strip_model()->GetActiveWebContents()) {
    return;
  }
  GURL url = browser_->tab_strip_model()->GetActiveWebContents()->GetURL();
  if (page_actions_excluded_from_logging_.find(url) ==
      page_actions_excluded_from_logging_.end()) {
    page_actions_excluded_from_logging_[url] = {};
  }
  std::vector<raw_ptr<PageActionIconView, VectorExperimental>>
      excluded_actions_on_page = page_actions_excluded_from_logging_[url];
  if (!view->ephemeral() || base::Contains(excluded_actions_on_page, view)) {
    return;
  }
  RecordOverallMetrics();
  RecordIndividualMetrics(GetIconType(view), view);
  page_actions_excluded_from_logging_[url].push_back(view);
}

void PageActionIconController::OnPageActionIconViewClicked(
    PageActionIconView* view) {
  if (!view->ephemeral()) {
    return;
  }
  RecordClickMetrics(GetIconType(view), view);
}

void PageActionIconController::ZoomChangedForActiveTab(bool can_show_bubble) {
  if (zoom_icon_)
    zoom_icon_->ZoomChangedForActiveTab(can_show_bubble);
}

std::vector<const PageActionIconView*>
PageActionIconController::GetPageActionIconViewsForTesting() const {
  std::vector<const PageActionIconView*> icon_views;
  base::ranges::transform(page_action_icon_views_,
                          std::back_inserter(icon_views),
                          &IconViews::value_type::second);
  return icon_views;
}

void PageActionIconController::OnDefaultZoomLevelChanged() {
  ZoomChangedForActiveTab(false);
}

void PageActionIconController::UpdateWebContents(
    content::WebContents* contents) {
  Observe(contents);
}

void PageActionIconController::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  page_actions_excluded_from_logging_.erase(
      navigation_handle->GetWebContents()->GetURL());
  max_actions_recorded_on_current_page_ = 0;
}

void PageActionIconController::PrimaryPageChanged(content::Page& page) {
  const GURL url = page.GetMainDocument().GetLastCommittedURL();
  RecordMetricsOnURLChange(url);
}

int PageActionIconController::VisibleEphemeralActionCount() const {
  return base::ranges::count_if(
      page_action_icon_views_,
      [](std::pair<PageActionIconType, PageActionIconView*> view) {
        return view.second->ephemeral() && view.second->GetVisible();
      });
}

void PageActionIconController::RecordMetricsOnURLChange(GURL url) {
  if (page_actions_excluded_from_logging_.find(url) ==
      page_actions_excluded_from_logging_.end()) {
    page_actions_excluded_from_logging_[url] = {};
  }
  std::vector<raw_ptr<PageActionIconView, VectorExperimental>>
      excluded_actions_on_page = page_actions_excluded_from_logging_[url];
  RecordOverallMetrics();
  for (auto icon_item : page_action_icon_views_) {
    if (!icon_item.second->ephemeral() || !icon_item.second->GetVisible() ||
        base::Contains(excluded_actions_on_page, icon_item.second)) {
      continue;
    }
    RecordIndividualMetrics(icon_item.first, icon_item.second);
    page_actions_excluded_from_logging_[url].push_back(icon_item.second);
  }
  base::UmaHistogramEnumeration("PageActionController.PagesWithActionsShown2",
                                PageActionPageEvent::kPageShown);
}

void PageActionIconController::RecordOverallMetrics() {
  int num_actions_shown = VisibleEphemeralActionCount();
  base::UmaHistogramExactLinear("PageActionController.NumberActionsShown2",
                                num_actions_shown, 20);
  // Record kActionShown if this is the first time an ephemeral action has been
  // shown on the current page.
  if (num_actions_shown > 0 && max_actions_recorded_on_current_page_ < 1) {
    base::UmaHistogramEnumeration("PageActionController.PagesWithActionsShown2",
                                  PageActionPageEvent::kActionShown);
  }
  // Record kMultipleActionsShown if this is the first time multiple ephemeral
  // actions have been shown on the current page. It is possible for this to
  // happen concurrently with the above if case, in the instance that a page is
  // loaded with multiple ephemeral actions immediately showing. kActionShown
  // and kMultipleActionsShown are not intended to be mutually exclusive, so in
  // this case we should log both.
  if (num_actions_shown > 1 && max_actions_recorded_on_current_page_ < 2) {
    base::UmaHistogramEnumeration("PageActionController.PagesWithActionsShown2",
                                  PageActionPageEvent::kMultipleActionsShown);
  }
  max_actions_recorded_on_current_page_ =
      std::max(num_actions_shown, max_actions_recorded_on_current_page_);
}

void PageActionIconController::RecordIndividualMetrics(
    PageActionIconType type,
    PageActionIconView* view) const {
  CHECK(view->ephemeral());
  base::UmaHistogramEnumeration("PageActionController.Icon.CTR2",
                                PageActionCTREvent::kShown);
  RecordCTRMetrics(view->name_for_histograms(), PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration("PageActionController.ActionTypeShown2", type);
}

void PageActionIconController::RecordClickMetrics(
    PageActionIconType type,
    PageActionIconView* view) const {
  CHECK(view->ephemeral());
  base::UmaHistogramEnumeration("PageActionController.Icon.CTR2",
                                PageActionCTREvent::kClicked);
  RecordCTRMetrics(view->name_for_histograms(), PageActionCTREvent::kClicked);
  base::UmaHistogramExactLinear(
      "PageActionController.Icon.NumberActionsShownWhenClicked",
      VisibleEphemeralActionCount(), 20);
}
