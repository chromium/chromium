// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {
// This is not the real height of the bubble, it is used only to initialize the
// view and the real height is sent by DiceWebSigninInterceptHandler and set on
// SetHeightAndShowWidget().
constexpr int kInterceptionBubbleBaseHeight = 500;
constexpr int kInterceptionBubbleWidth = 290;
constexpr int kInterceptionChromeSigninBubbleWidth = 320;

AvatarToolbarButton* GetAvatarToolbarButton(const Browser& browser) {
  return BrowserView::GetBrowserViewForBrowser(&browser)
      ->toolbar_button_provider()
      ->GetAvatarToolbarButton();
}

std::u16string InterceptionTypeToIdentityPillText(
    WebSigninInterceptor::SigninInterceptionType interception_type) {
  switch (interception_type) {
    case WebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
      return l10n_util::GetStringUTF16(
          IDS_SIGNIN_DICE_WEB_INTERCEPT_AVATAR_BUTTON_SWITCH_PROFILE_TEXT);
    case WebSigninInterceptor::SigninInterceptionType::kChromeSignin:
      return l10n_util::GetStringUTF16(
          IDS_AVATAR_BUTTON_INTERCEPT_BUBBLE_CHROME_SIGNIN_TEXT);
    case WebSigninInterceptor::SigninInterceptionType::kMultiUser:
    case WebSigninInterceptor::SigninInterceptionType::kEnterprise:
      return l10n_util::GetStringUTF16(
          IDS_SIGNIN_DICE_WEB_INTERCEPT_AVATAR_BUTTON_SEPARATE_BROWSING_TEXT);
    case WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced:
    case WebSigninInterceptor::SigninInterceptionType::
        kEnterpriseAcceptManagement:
    case WebSigninInterceptor::SigninInterceptionType::kEnterpriseOIDC:
    case WebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced:
      // These intercept type do not show a bubble and should not need to change
      // the identity pill text.
      NOTREACHED();
  }
}

std::optional<std::u16string> InteractionTypeToIdentityPillAccessibilityLabel(
    WebSigninInterceptor::SigninInterceptionType interception_type) {
  switch (interception_type) {
    case WebSigninInterceptor::SigninInterceptionType::kChromeSignin:
      if (switches::kInterceptBubblesDismissibleByAvatarButton.Get()) {
        return l10n_util::GetStringUTF16(
            IDS_AVATAR_BUTTON_INTERCEPT_BUBBLE_CHROME_SIGNIN_ACCESSIBILITY_LABEL);
      } else {
        return std::nullopt;
      }
    case WebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
    case WebSigninInterceptor::SigninInterceptionType::kMultiUser:
    case WebSigninInterceptor::SigninInterceptionType::kEnterprise:
      return std::nullopt;
    case WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced:
    case WebSigninInterceptor::SigninInterceptionType::
        kEnterpriseAcceptManagement:
    case WebSigninInterceptor::SigninInterceptionType::kEnterpriseOIDC:
    case WebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced:
      // These intercept type do not show a bubble and should not need to change
      // the identity pill text.
      NOTREACHED();
  }
}

GURL GetURLForInterceptionType(bool is_chrome_signin) {
  return is_chrome_signin
             ? GURL(chrome::kChromeUIDiceWebSigninInterceptChromeSigninURL)
             : GURL(chrome::kChromeUIDiceWebSigninInterceptURL);
}

int GetBubbleFixedWidthForInterceptionType(bool is_chrome_signin) {
  return is_chrome_signin ? kInterceptionChromeSigninBubbleWidth
                          : kInterceptionBubbleWidth;
}

void RecordMetricsChromeSigninInterceptStarted() {
  auto access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE;
  RecordSigninImpressionUserActionForAccessPoint(access_point);
  signin_metrics::LogSignInOffered(access_point);
}

std::string_view GetChromeSigninReactionString(
    SigninInterceptionResult result) {
  switch (result) {
    case SigninInterceptionResult::kAccepted:
      return "Accepted";
    case SigninInterceptionResult::kDeclined:
      return "Declined";
    case SigninInterceptionResult::kDismissed:
      return "Dismissed";
    case SigninInterceptionResult::kAcceptedWithExistingProfile:
    case SigninInterceptionResult::kIgnored:
    case SigninInterceptionResult::kNotDisplayed:
      NOTREACHED() << "These results should not be recorded or not "
                      "expected for the Chrome Signin Bubble.";
  }
}

void RecordChromeSigninInterceptResult(base::TimeTicks start_time,
                                       SigninInterceptionResult result) {
  CHECK_NE(start_time, base::TimeTicks());
  constexpr std::string_view kBaseResponseTimeHistogram =
      "Signin.Intercept.ChromeSignin.ResponseTime";

  std::string_view reaction = GetChromeSigninReactionString(result);
  std::string reaction_time_histogram_name =
      base::StrCat({kBaseResponseTimeHistogram, reaction});

  base::UmaHistogramMediumTimes(reaction_time_histogram_name,
                                base::TimeTicks::Now() - start_time);

  // Only record user action on successful signin inputs.
  if (result == SigninInterceptionResult::kAccepted) {
    RecordSigninUserActionForAccessPoint(
        signin_metrics::AccessPoint::
            ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE);
  }
}

// New changes only in Full design.
bool ShouldUseFullDesign() {
  return switches::IsExplicitBrowserSigninUIOnDesktopEnabled();
}

void RecordDismissReason(
    WebSigninInterceptor::SigninInterceptionType interception_type,
    SigninInterceptionDismissReason reason) {
  // Only those types show a bubble that would be dismissible.
  CHECK(interception_type ==
            WebSigninInterceptor::SigninInterceptionType::kChromeSignin ||
        interception_type ==
            WebSigninInterceptor::SigninInterceptionType::kEnterprise ||
        interception_type ==
            WebSigninInterceptor::SigninInterceptionType::kMultiUser ||
        interception_type ==
            WebSigninInterceptor::SigninInterceptionType::kProfileSwitch)
      << "interception_type of value \"" << static_cast<int>(interception_type)
      << "\" is not expected to have a dismiss value recorded.";

  static const char kBaseHistogramName[] =
      "Signin.Intercept.BubbleDismissReason";
  base::UmaHistogramEnumeration(kBaseHistogramName, reason);

  std::string type_histogram_name = base::StrCat(
      {kBaseHistogramName, DiceWebSigninInterceptorDelegate::GetHistogramSuffix(
                               interception_type)});
  base::UmaHistogramEnumeration(type_histogram_name, reason);
}

}  // namespace

DiceWebSigninInterceptionBubbleView::~DiceWebSigninInterceptionBubbleView() {
  // Cancel if the bubble is destroyed without user interaction.
  if (callback_) {
    RecordInterceptionResult(bubble_parameters_, profile_,
                             SigninInterceptionResult::kIgnored);
    // The callback may synchronously delete a handle, which would attempt to
    // close this bubble while it is being destroyed. Invalidate the handles now
    // to prevent this.
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(SigninInterceptionResult::kIgnored);
  }
}

// static
std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptionBubbleView::CreateBubble(
    Browser* browser,
    views::View* anchor_view,
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  auto interception_bubble =
      base::WrapUnique(new DiceWebSigninInterceptionBubbleView(
          browser, anchor_view, bubble_parameters, std::move(callback)));
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle =
      interception_bubble->GetHandle();
  // The widget is owned by the views system and shown after the view is loaded
  // and the final height of the bubble is sent from
  // DiceWebSigninInterceptHandler.
  views::BubbleDialogDelegateView::CreateBubble(std::move(interception_bubble));

  return handle;
}

DiceWebSigninInterceptionBubbleView::ScopedHandle::~ScopedHandle() {
  if (!bubble_) {
    return;  // The bubble was already closed, do nothing.
  }
  views::Widget* widget = bubble_->GetWidget();
  if (!widget) {
    return;
  }
  widget->CloseWithReason(
      bubble_->GetAccepted() ? views::Widget::ClosedReason::kAcceptButtonClicked
                             : views::Widget::ClosedReason::kUnspecified);
}

DiceWebSigninInterceptionBubbleView::ScopedHandle::ScopedHandle(
    base::WeakPtr<DiceWebSigninInterceptionBubbleView> bubble)
    : bubble_(std::move(bubble)) {
  DCHECK(bubble_);
}

// static
void DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    Profile* profile,
    SigninInterceptionResult result) {
  DiceWebSigninInterceptorDelegate::RecordInterceptionResult(bubble_parameters,
                                                             profile, result);
}

bool DiceWebSigninInterceptionBubbleView::GetAccepted() const {
  return accepted_;
}

content::WebContents* DiceWebSigninInterceptionBubbleView::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // Allows the Signin Interception bubble to open links in a new tab.
  if (browser_) {
    chrome::AddWebContents(browser_.get(), source, std::move(new_contents),
                           target_url, disposition, window_features);
  }
  return nullptr;
}

DiceWebSigninInterceptionBubbleView::DiceWebSigninInterceptionBubbleView(
    Browser* browser,
    views::View* anchor_view,
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT),
      profile_keep_alive_(
          browser->profile(),
          ProfileKeepAliveOrigin::kDiceWebSigninInterceptionBubble),
      browser_(browser->AsWeakPtr()),
      profile_(browser->profile()),
      bubble_parameters_(bubble_parameters),
      callback_(std::move(callback)) {
  DCHECK(browser_);
  DCHECK(callback_);
  set_close_on_deactivate(false);

  // Create the web view in the native bubble.
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(browser->profile());
  web_view->LoadInitialURL(GetURLForInterceptionType(IsChromeSignin()));
  web_view->GetWebContents()->SetDelegate(this);
  web_view->SetPreferredSize(
      gfx::Size(GetBubbleFixedWidthForInterceptionType(IsChromeSignin()),
                kInterceptionBubbleBaseHeight));
  DiceWebSigninInterceptUI* web_ui = web_view->GetWebContents()
                                         ->GetWebUI()
                                         ->GetController()
                                         ->GetAs<DiceWebSigninInterceptUI>();
  SetInitiallyFocusedView(web_view.get());
  DCHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(
      bubble_parameters,
      base::BindOnce(
          &DiceWebSigninInterceptionBubbleView::SetHeightAndShowWidget,
          base::Unretained(this)),
      base::BindOnce(&DiceWebSigninInterceptionBubbleView::OnWebUIUserChoice,
                     base::Unretained(this)));
  web_view_ = web_view.get();
  AddChildView(std::move(web_view));

  set_margins(gfx::Insets());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void DiceWebSigninInterceptionBubbleView::SetHeightAndShowWidget(int height) {
  web_view_->SetPreferredSize(gfx::Size(
      GetBubbleFixedWidthForInterceptionType(IsChromeSignin()), height));
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  GetWidget()->Show();

  if (ShouldUseFullDesign() || IsChromeSignin()) {
    // Explicitly add corners to the inner web view to match the bubble corners.
    // This has to be done since we removed the margins of the bubble view,
    // which would create an overlap of the web view on top of the bubble empty
    // corners.
    web_view_->holder()->SetCornerRadii(
        gfx::RoundedCornersF(GetCornerRadius()));
  }

  ApplyAvatarButtonEffects();

  if (IsChromeSignin()) {
    chrome_signin_bubble_shown_time_ = base::TimeTicks::Now();
    RecordMetricsChromeSigninInterceptStarted();
  }
}

std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptionBubbleView::GetHandle() {
  return std::make_unique<ScopedHandle>(weak_factory_.GetWeakPtr());
}

void DiceWebSigninInterceptionBubbleView::OnWebUIUserChoice(
    SigninInterceptionUserChoice user_choice) {
  SigninInterceptionResult result;
  switch (user_choice) {
    case SigninInterceptionUserChoice::kAccept:
      result = SigninInterceptionResult::kAccepted;
      break;
    case SigninInterceptionUserChoice::kDecline:
      result = SigninInterceptionResult::kDeclined;
      break;
  }
  OnInterceptionResult(result);
}

void DiceWebSigninInterceptionBubbleView::OnInterceptionResult(
    SigninInterceptionResult result) {
  accepted_ = result == SigninInterceptionResult::kAccepted;

  if (IsChromeSignin()) {
    RecordChromeSigninInterceptResult(chrome_signin_bubble_shown_time_, result);
  }

  RecordInterceptionResult(bubble_parameters_, profile_, result);

  ClearAvatarButtonEffects();

  std::move(callback_).Run(result);
  if (!accepted_) {
    // Only close the dialog when the user declined. If the user accepted the
    // dialog displays a spinner until the handle is released.
    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCancelButtonClicked);
  }
}

content::WebContents*
DiceWebSigninInterceptionBubbleView::GetBubbleWebContentsForTesting() {
  return web_view_->GetWebContents();
}

bool DiceWebSigninInterceptionBubbleView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.dom_key == ui::DomKey::ESCAPE && ShouldUseFullDesign()) {
    Dismiss(SigninInterceptionDismissReason::kEscKey);
    return true;
  }

  return false;
}

void DiceWebSigninInterceptionBubbleView::Dismiss(
    SigninInterceptionDismissReason reason) {
  CHECK(ShouldUseFullDesign());

  RecordDismissReason(bubble_parameters_.interception_type, reason);
  OnInterceptionResult(SigninInterceptionResult::kDismissed);
}

bool DiceWebSigninInterceptionBubbleView::IsChromeSignin() const {
  return bubble_parameters_.interception_type ==
         WebSigninInterceptor::SigninInterceptionType::kChromeSignin;
}

void DiceWebSigninInterceptionBubbleView::ApplyAvatarButtonEffects() {
  // The effect is split into two components: text and button action.
  //
  // For the text component:
  // The Chrome Signin bubble always show the expected text, however the rest of
  // the bubbles only show it when the full experiment is run.
  //
  // For the action component:
  // If the full experiment is run with the feature param
  // `bubble_dismissible_by_avatar_button` enabled, all the bubbles will have
  // the avatar button to dismiss the bubble. Otherwise, only the Chrome Sign in
  // bubble will be deactivated.
  //
  // Changes done in this method should also be reflected in the method that
  // resets the effects `ClearAvatarButtonEffects()`.

  AvatarToolbarButton* button = GetAvatarToolbarButton(*browser_);
  // Avatar text behavior
  if (ShouldUseFullDesign() || IsChromeSignin()) {
    // Adapt the identity pill, show the appropriate intercept text and
    // highlight the button as long as the text is shown.
    hide_avatar_text_callback_ = button->ShowExplicitText(
        InterceptionTypeToIdentityPillText(
            bubble_parameters_.interception_type),
        InteractionTypeToIdentityPillAccessibilityLabel(
            bubble_parameters_.interception_type));
  }
  // Avatar Button action behavior
  if (ShouldUseFullDesign() &&
      switches::kInterceptBubblesDismissibleByAvatarButton.Get()) {
    reset_avatar_button_action_callback_ =
        button->SetExplicitButtonAction(base::BindRepeating(
            &DiceWebSigninInterceptionBubbleView::Dismiss,
            weak_factory_.GetWeakPtr(),
            /*reason=*/SigninInterceptionDismissReason::kIdentityPillPressed));
  } else if (IsChromeSignin()) {
    button->SetButtonActionDisabled(true);
  }
}

void DiceWebSigninInterceptionBubbleView::ClearAvatarButtonEffects() {
  // Main logic described in the apply function.
  // Changes done in this method should also be reflected in the method that
  // applies the effects `ApplyAvatarButtonEffects()`.

  AvatarToolbarButton* button = GetAvatarToolbarButton(*browser_);
  // Avatar text behavior
  if (ShouldUseFullDesign() || IsChromeSignin()) {
    hide_avatar_text_callback_.RunAndReset();
  }
  // Avatar Button action behavior
  if (ShouldUseFullDesign()) {
    reset_avatar_button_action_callback_.RunAndReset();
  } else if (IsChromeSignin()) {
    button->SetButtonActionDisabled(false);
  }
}

// DiceWebSigninInterceptorDelegate --------------------------------------------

// static
bool DiceWebSigninInterceptorDelegate::IsSigninInterceptionSupportedInternal(
    const Browser& browser) {
  // Some browsers, such as web apps, don't have an avatar toolbar button to
  // anchor the bubble. Even if a web app has an avatar toolbar button, we
  // still don't support signin interception.
  return GetAvatarToolbarButton(browser) != nullptr &&
         !web_app::AppBrowserController::IsWebApp(&browser);
}

std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubbleInternal(
    Browser* browser,
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  DCHECK(browser);

  views::View* anchor_view = GetAvatarToolbarButton(*browser);
  DCHECK(anchor_view);
  return DiceWebSigninInterceptionBubbleView::CreateBubble(
      browser, anchor_view, bubble_parameters, std::move(callback));
}

BEGIN_METADATA(DiceWebSigninInterceptionBubbleView)
ADD_READONLY_PROPERTY_METADATA(bool, Accepted)
END_METADATA
