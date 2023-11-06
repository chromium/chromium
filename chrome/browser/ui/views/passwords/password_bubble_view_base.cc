// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_view.h"
#include "chrome/browser/ui/views/passwords/move_to_account_store_bubble_view.h"
#include "chrome/browser/ui/views/passwords/password_add_username_view.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "chrome/browser/ui/views/passwords/password_generation_confirmation_view.h"
#include "chrome/browser/ui/views/passwords/password_save_unsynced_credentials_locally_view.h"
#include "chrome/browser/ui/views/passwords/password_save_update_view.h"
#include "chrome/browser/ui/views/passwords/post_save_compromised_bubble_view.h"
#include "chrome/browser/ui/views/passwords/shared_passwords_notification_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "ui/views/controls/button/button.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/views/passwords/biometric_authentication_confirmation_bubble_view.h"
#include "chrome/browser/ui/views/passwords/biometric_authentication_for_filling_bubble_view.h"
#endif

// static
PasswordBubbleViewBase* PasswordBubbleViewBase::g_manage_passwords_bubble_ =
    nullptr;

// static
void PasswordBubbleViewBase::ShowBubble(content::WebContents* web_contents,
                                        DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(!g_manage_passwords_bubble_ ||
         !g_manage_passwords_bubble_->GetWidget()->IsVisible());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ToolbarButtonProvider* button_provider =
      browser_view->toolbar_button_provider();
  views::View* anchor_view =
      button_provider->GetAnchorView(PageActionIconType::kManagePasswords);

  PasswordBubbleViewBase* bubble =
      CreateBubble(web_contents, anchor_view, reason);
  DCHECK(bubble);
  DCHECK_EQ(bubble, g_manage_passwords_bubble_);
  // TODO(crbug.com/1305276): In non-DCHECK mode we could fall through here and
  // hard-crash if we requested a bubble and were in the wrong state. In the
  // meantime we will abort if we did not create a bubble.
  if (!g_manage_passwords_bubble_)
    return;

  g_manage_passwords_bubble_->SetHighlightedButton(
      button_provider->GetPageActionIconView(
          PageActionIconType::kManagePasswords));

  views::BubbleDialogDelegateView::CreateBubble(g_manage_passwords_bubble_);

  g_manage_passwords_bubble_->ShowForReason(reason);
}

// static
PasswordBubbleViewBase* PasswordBubbleViewBase::CreateBubble(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason) {
  PasswordBubbleViewBase* view = nullptr;
  password_manager::ui::State model_state =
      PasswordsModelDelegateFromWebContents(web_contents)->GetState();
  if (model_state == password_manager::ui::MANAGE_STATE) {
      view = new ManagePasswordsView(web_contents, anchor_view);
  } else if (model_state == password_manager::ui::AUTO_SIGNIN_STATE) {
    view = new PasswordAutoSignInView(web_contents, anchor_view);
  } else if (model_state == password_manager::ui::SAVE_CONFIRMATION_STATE ||
             model_state == password_manager::ui::UPDATE_CONFIRMATION_STATE) {
    if (base::FeatureList::IsEnabled(
            password_manager::features::
                kNewConfirmationBubbleForGeneratedPasswords)) {
      view = new ManagePasswordsView(web_contents, anchor_view);
    } else {
      view = new PasswordGenerationConfirmationView(web_contents, anchor_view,
                                                    reason);
    }
  } else if (model_state ==
                 password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
             model_state == password_manager::ui::PENDING_PASSWORD_STATE) {
    view = new PasswordSaveUpdateView(web_contents, anchor_view, reason);
  } else if (model_state == password_manager::ui::
                                WILL_DELETE_UNSYNCED_ACCOUNT_PASSWORDS_STATE) {
    view = new PasswordSaveUnsyncedCredentialsLocallyView(web_contents,
                                                          anchor_view);
  } else if (model_state ==
             password_manager::ui::CAN_MOVE_PASSWORD_TO_ACCOUNT_STATE) {
    view = new MoveToAccountStoreBubbleView(web_contents, anchor_view);
  } else if (model_state == password_manager::ui::PASSWORD_UPDATED_SAFE_STATE ||
             model_state ==
                 password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX) {
    view = new PostSaveCompromisedBubbleView(web_contents, anchor_view);
  } else if (model_state ==
             password_manager::ui::GENERATED_PASSWORD_CONFIRMATION_STATE) {
    view = new PasswordAddUsernameView(web_contents, anchor_view, reason);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  } else if (model_state ==
             password_manager::ui::BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE) {
    view = new BiometricAuthenticationForFillingBubbleView(
        web_contents, anchor_view,
        Profile::FromBrowserContext(web_contents->GetBrowserContext())
            ->GetPrefs(),
        reason);
  } else if (model_state == password_manager::ui::
                                BIOMETRIC_AUTHENTICATION_CONFIRMATION_STATE) {
    view = new BiometricAuthenticationConfirmationBubbleView(web_contents,
                                                             anchor_view);
#endif
  } else if (model_state ==
             password_manager::ui::NOTIFY_RECEIVED_SHARED_CREDENTIALS) {
    view = new SharedPasswordsNotificationView(web_contents, anchor_view);
  } else {
    NOTREACHED_NORETURN();
  }

  g_manage_passwords_bubble_ = view;
  return view;
}

// static
void PasswordBubbleViewBase::CloseCurrentBubble() {
  if (g_manage_passwords_bubble_) {
    // It can be the case that a password bubble is being closed while another
    // password bubble is being opened. The metrics recorder can be shared
    // between them and it doesn't understand the sequence [open1, open2,
    // close1, close2]. Therefore, we reset the model early (before the bubble
    // destructor) to get the following sequence of events [open1, close1,
    // open2, close2].
    PasswordBubbleControllerBase* controller =
        g_manage_passwords_bubble_->GetController();
    DCHECK(controller);
    controller->OnBubbleClosing();
    g_manage_passwords_bubble_->GetWidget()->Close();
  }
}

// static
void PasswordBubbleViewBase::ActivateBubble() {
  DCHECK(g_manage_passwords_bubble_);
  DCHECK(g_manage_passwords_bubble_->GetWidget()->IsVisible());
  g_manage_passwords_bubble_->GetWidget()->Activate();
}

const content::WebContents* PasswordBubbleViewBase::GetWebContents() const {
  const PasswordBubbleControllerBase* controller = GetController();
  DCHECK(controller);
  return controller->GetWebContents();
}

PasswordBubbleViewBase::PasswordBubbleViewBase(
    content::WebContents* web_contents,
    views::View* anchor_view,
    bool easily_dismissable)
    : LocationBarBubbleDelegateView(anchor_view, web_contents) {
  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_close_on_deactivate(easily_dismissable);
}

PasswordBubbleViewBase::~PasswordBubbleViewBase() {
  if (g_manage_passwords_bubble_ == this)
    g_manage_passwords_bubble_ = nullptr;
}

void PasswordBubbleViewBase::SetBubbleHeader(int light_image_id,
                                             int dark_image_id) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(light_image_id),
      *bundle.GetImageSkiaNamed(dark_image_id),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));

  gfx::Size preferred_size = image_view->GetPreferredSize();
  if (preferred_size.width()) {
    float scale =
        static_cast<float>(ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_BUBBLE_PREFERRED_WIDTH)) /
        preferred_size.width();
    preferred_size = gfx::ScaleToRoundedSize(preferred_size, scale);
    image_view->SetImageSize(preferred_size);
  }
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

void PasswordBubbleViewBase::Init() {
  LocationBarBubbleDelegateView::Init();
  const PasswordBubbleControllerBase* controller = GetController();
  DCHECK(controller);
  SetTitle(controller->GetTitle());
  SetShowTitle(!controller->GetTitle().empty());
}
