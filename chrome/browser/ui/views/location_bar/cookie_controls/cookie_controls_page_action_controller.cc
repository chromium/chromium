// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"

#include "base/callback_list.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

namespace {

void RecordOpenedAction(bool icon_visible, CookieControlsState controls_state) {
  if (!icon_visible) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.Bubble.UnknownState.Opened"));
  } else if (controls_state == CookieControlsState::kBlocked3pc) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.Bubble.CookiesBlocked.Opened"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.Bubble.CookiesAllowed.Opened"));
  }
}

class BubbleDelegateImpl
    : public CookieControlsPageActionController::BubbleDelegate {
 public:
  explicit BubbleDelegateImpl(tabs::TabInterface& tab_interface)
      : tab_interface_(tab_interface) {}
  BubbleDelegateImpl(const BubbleDelegateImpl&) = delete;
  BubbleDelegateImpl& operator=(const BubbleDelegateImpl&) = delete;
  ~BubbleDelegateImpl() override = default;

  bool HasBubble() override { return GetBubbleCoordinator().GetBubble(); }

  void ShowBubble(
      ToolbarButtonProvider* toolbar_button_provider,
      content::WebContents* web_contents,
      content_settings::CookieControlsController* controller) override {
    return GetBubbleCoordinator().ShowBubble(toolbar_button_provider,
                                             web_contents, controller);
  }

  base::CallbackListSubscription RegisterBubbleClosingCallback(
      base::RepeatingClosure callback) override {
    return GetBubbleCoordinator().RegisterBubbleClosingCallback(
        std::move(callback));
  }

 private:
  CookieControlsBubbleCoordinator& GetBubbleCoordinator() {
    auto* const bwi = tab_interface_->GetBrowserWindowInterface();
    CHECK(bwi);
    CookieControlsBubbleCoordinator* const coordinator =
        CookieControlsBubbleCoordinator::From(bwi);
    CHECK(coordinator);
    return *coordinator;
  }

  const raw_ref<tabs::TabInterface> tab_interface_;
};

int GetLabelForStatus(CookieControlsState controls_state,
                      CookieBlocking3pcdStatus blocking_status) {
  if (controls_state == CookieControlsState::kAllowed3pc) {
    return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL;
  }
  return blocking_status == CookieBlocking3pcdStatus::kLimited
             ? IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_LIMITED_LABEL
             : IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL;
}

const gfx::VectorIcon& GetVectorIcon(CookieControlsState controls_state) {
  return controls_state == CookieControlsState::kBlocked3pc
             ? views::kEyeCrossedRefreshIcon
             : views::kEyeRefreshIcon;
}
}  // namespace

DEFINE_USER_DATA(CookieControlsPageActionController);

CookieControlsPageActionController::CookieControlsPageActionController(
    tabs::TabInterface& tab_interface,
    Profile& profile,
    page_actions::PageActionController& page_action_controller)
    : PageActionObserver(kActionShowCookieControls),
      tab_(tab_interface),
      page_action_controller_(page_action_controller),
      cookie_controls_controller_(
          std::make_unique<content_settings::CookieControlsController>(
              CookieSettingsFactory::GetForProfile(&profile),
              profile.IsOffTheRecord() ? CookieSettingsFactory::GetForProfile(
                                             profile.GetOriginalProfile())
                                       : nullptr,
              HostContentSettingsMapFactory::GetForProfile(&profile),
              TrackingProtectionSettingsFactory::GetForProfile(&profile),
              profile.IsIncognitoProfile())),
      bubble_delegate_(std::make_unique<BubbleDelegateImpl>(tab_interface)),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  CHECK(IsPageActionMigrated(PageActionIconType::kCookieControls));
  RegisterAsPageActionObserver(page_action_controller_.get());
}

CookieControlsPageActionController::~CookieControlsPageActionController() =
    default;

// static
CookieControlsPageActionController* CookieControlsPageActionController::From(
    tabs::TabInterface& tab) {
  return Get(tab.GetUnownedUserDataHost());
}

void CookieControlsPageActionController::Init() {
  controller_observation_.Observe(cookie_controls_controller_.get());

  // These will get updated naturally.
  icon_status_.controls_state = CookieControlsState::kHidden;
  icon_status_.icon_visible = false;
  icon_status_.should_highlight = false;
  icon_status_.blocking_status = CookieBlocking3pcdStatus::kMaxValue;

  cookie_controls_controller_->Update(tab_->GetContents());

  will_discard_contents_subscription_ =
      tab_->RegisterWillDiscardContents(base::BindRepeating(
          [](content_settings::CookieControlsController& cookies_controller,
             tabs::TabInterface* tab, content::WebContents* old_contents,
             content::WebContents* new_contents) {
            if (new_contents) {
              cookies_controller.Update(new_contents);
            }
          },
          std::ref(*cookie_controls_controller_)));

  tab_deactivation_subscription_ =
      tab_->RegisterWillDeactivate(base::BindRepeating(
          [](content_settings::CookieControlsController& cookies_controller,
             tabs::TabInterface* tab) {
            cookies_controller.OnBubbleCloseTriggered();
          },
          std::ref(*cookie_controls_controller_)));

  tab_will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      [](content_settings::CookieControlsController& cookies_controller,
         tabs::TabInterface* tab,
         tabs::TabInterface::DetachReason detach_reason) {
        if (tab->IsActivated()) {
          cookies_controller.OnBubbleCloseTriggered();
        }
      },
      std::ref(*cookie_controls_controller_)));

  bubble_will_close_subscription_ =
      bubble_delegate_->RegisterBubbleClosingCallback(base::BindRepeating(
          &CookieControlsPageActionController::OnBubbleClosed,
          base::Unretained(this)));
}

void CookieControlsPageActionController::OnPageActionChipShown(
    const page_actions::PageActionState& page_action) {
  hide_chip_timer_.Start(
      FROM_HERE, base::Seconds(12),
      base::BindOnce(
          [](page_actions::PageActionController& controller) {
            controller.HideSuggestionChip(kActionShowCookieControls);
          },
          std::ref(page_action_controller_.get())));
}

void CookieControlsPageActionController::OnCookieControlsIconStatusChanged(
    bool icon_visible,
    CookieControlsState controls_state,
    CookieBlocking3pcdStatus blocking_status,
    bool should_highlight) {
  const bool controls_state_changed =
      controls_state != icon_status_.controls_state;
  const bool should_update_icon =
      controls_state_changed ||
      blocking_status != icon_status_.blocking_status ||
      should_highlight != icon_status_.should_highlight;
  icon_status_ = CookieControlsIconStatus{
      .icon_visible = icon_visible,
      .controls_state = controls_state,
      .blocking_status = blocking_status,
      .should_highlight = should_highlight,
  };

  UpdateIconVisibility();
  if (!should_update_icon) {
    return;
  }

  page_action_controller_->OverrideImage(
      kActionShowCookieControls, ui::ImageModel::FromVectorIcon(GetVectorIcon(
                                     icon_status_.controls_state)));

  const std::u16string label = GetLabelForState();
  page_action_controller_->OverrideTooltip(kActionShowCookieControls, label);
  if (controls_state_changed) {
    page_action_controller_->OverrideText(kActionShowCookieControls, label);
  }

  if (!icon_status_.icon_visible || !icon_status_.should_highlight ||
      icon_status_.controls_state != CookieControlsState::kBlocked3pc ||
      bubble_delegate_->HasBubble()) {
    return;
  }
  if (icon_status_.blocking_status == CookieBlocking3pcdStatus::kNotIn3pcd) {
    if (auto* user_education = BrowserUserEducationInterface::From(
            tab_->GetBrowserWindowInterface())) {
      MaybeShowIPH(*user_education);
    }
  } else if (!IsManagedIPHActive()) {
    page_action_controller_->OverrideText(
        kActionShowCookieControls,
        l10n_util::GetStringUTF16(
            IDS_TRACKING_PROTECTION_PAGE_ACTION_SITE_NOT_WORKING_LABEL));
    page_action_controller_->ShowSuggestionChip(
        kActionShowCookieControls, page_actions::SuggestionChipConfig{
                                       .should_animate = true,
                                       .should_announce_chip = true,
                                   });
  }
}

void CookieControlsPageActionController::
    OnFinishedPageReloadWithChangedSettings() {
  if (ShouldShowIcon()) {
    const std::u16string label = GetLabelForState();
    page_action_controller_->OverrideText(kActionShowCookieControls, label);
    page_action_controller_->OverrideTooltip(kActionShowCookieControls, label);
    // Animate the label to provide a visual confirmation to the user that
    // their protection status on the site has changed.
    // TODO(crbug.com/376283777): Ensure that Mac voiceover doesn't trigger
    // here.
    page_action_controller_->ShowSuggestionChip(
        kActionShowCookieControls,
        {.should_animate = true, .should_announce_chip = true});
  }
}

bool CookieControlsPageActionController::ShouldShowIcon() const {
  return icon_status_.icon_visible || bubble_delegate_->HasBubble();
}

void CookieControlsPageActionController::UpdateIconVisibility() {
  if (!ShouldShowIcon()) {
    // Hiding the page action will close the IPH, if any, which will be
    // re-entrant into the page action system when removing activity in the
    // handler. Reset IPH activity up front to avoid this.
    iph_activity_.reset();
    page_action_controller_->HideSuggestionChip(kActionShowCookieControls);
    page_action_controller_->Hide(kActionShowCookieControls);
    return;
  }
  page_action_controller_->Show(kActionShowCookieControls);
}

std::u16string CookieControlsPageActionController::GetLabelForState() const {
  return l10n_util::GetStringUTF16(GetLabelForStatus(
      icon_status_.controls_state, icon_status_.blocking_status));
}

bool CookieControlsPageActionController::IsManagedIPHActive() const {
  auto* user_education =
      BrowserUserEducationInterface::From(tab_->GetBrowserWindowInterface());
  if (!user_education) {
    return false;
  }
  return user_education->IsFeaturePromoActive(
             feature_engagement::kIPHCookieControlsFeature) ||
         user_education->IsFeaturePromoQueued(
             feature_engagement::kIPHCookieControlsFeature);
}

void CookieControlsPageActionController::OnShowPromoResult(
    user_education::FeaturePromoResult result) {
  if (result) {
    iph_activity_ =
        page_action_controller_->AddActivity(kActionShowCookieControls);
    return;
  }
  // If we attempted to show the IPH but failed, instead try animating.
  page_action_controller_->ShowSuggestionChip(
      kActionShowCookieControls, page_actions::SuggestionChipConfig{
                                     .should_animate = true,
                                     .should_announce_chip = true,
                                 });
}

void CookieControlsPageActionController::OnIPHClosed() {
  iph_activity_.reset();
}

void CookieControlsPageActionController::OnBubbleClosed() {
  UpdateIconVisibility();
}

void CookieControlsPageActionController::MaybeShowIPH(
    BrowserUserEducationInterface& user_education) {
  user_education::FeaturePromoParams params(
      feature_engagement::kIPHCookieControlsFeature);
  params.show_promo_result_callback =
      base::BindOnce(&CookieControlsPageActionController::OnShowPromoResult,
                     weak_ptr_factory_.GetWeakPtr());
  params.close_callback =
      base::BindOnce(&CookieControlsPageActionController::OnIPHClosed,
                     weak_ptr_factory_.GetWeakPtr());

  user_education.MaybeShowFeaturePromo(std::move(params));

  // Note: originally we would animate here based on whether the promo showed,
  // but since promos are show asynchronously, the options are:
  //  - Always animate; if the IPH shows it shows
  //  - Always wait until we get a yes or no answer from the promo system before
  //    deciding whether to animate
  // Since most of the time the result should come back quickly, and if it
  // doesn't, it's because the user is doing something else or there is another
  // promo showing, for now, we choose the later option.
}

void CookieControlsPageActionController::ExecutePageAction(
    ToolbarButtonProvider* toolbar_button_provider) {
  CHECK(ShouldShowIcon());
  if (auto* user_education = BrowserUserEducationInterface::From(
          tab_->GetBrowserWindowInterface())) {
    // Need to close IPH before opening bubble view, as on some platforms
    // closing the IPH bubble can cause activation to move between windows, and
    // cookie control bubble is close-on-deactivate.
    user_education->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHCookieControlsFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
  bubble_delegate_->ShowBubble(toolbar_button_provider, tab_->GetContents(),
                               cookie_controls_controller_.get());

  RecordOpenedAction(icon_status_.icon_visible, icon_status_.controls_state);
}
