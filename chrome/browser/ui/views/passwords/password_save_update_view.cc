// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_update_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/browser/ui/views/promos/autofill_bubble_signin_promo_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/editable_combobox/editable_password_combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Initiates a `MovePasswordToAccountStoreHelper`, which takes care of moving
// the `form` from profile to account store.
void MovePasswordToAccount(
    const password_manager::PasswordForm& form,
    password_manager::metrics_util::MoveToAccountStoreTrigger trigger,
    content::WebContents* web_contents) {
  PasswordsModelDelegateFromWebContents(web_contents)
      ->MovePendingPasswordToAccountStoreUsingHelper(form, trigger);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace

PasswordSaveUpdateView::PasswordSaveUpdateView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/reason == USER_GESTURE),
      controller_(
          PasswordsModelDelegateFromWebContents(web_contents),
          reason == AUTOMATIC
              ? PasswordBubbleControllerBase::DisplayReason::kAutomatic
              : PasswordBubbleControllerBase::DisplayReason::kUserAction),
      is_update_bubble_(controller_.state() ==
                        password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
  DCHECK(controller_.state() == password_manager::ui::PENDING_PASSWORD_STATE ||
         controller_.state() ==
             password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);

  const password_manager::PasswordForm& password_form =
      controller_.pending_password();
  if (password_form.IsFederatedCredential()) {
    // The credential to be saved doesn't contain password but just the identity
    // provider (e.g. "Sign in with Google"). Thus, the layout is different.
    views::FlexLayout* flex_layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                                DISTANCE_CONTROL_LIST_VERTICAL),
                            0));

    const auto titles = GetCredentialLabelsForAccountChooser(password_form);
    AddChildView(
        std::make_unique<CredentialsItemView>(
            views::Button::PressedCallback(), titles.first, titles.second,
            &password_form, GetURLLoaderForMainFrame(web_contents).get(),
            web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin()))
        ->SetEnabled(false);
  } else {
    std::unique_ptr<views::EditableCombobox> username_dropdown =
        CreateUsernameEditableCombobox(password_form);
    username_dropdown->SetCallback(base::BindRepeating(
        &PasswordSaveUpdateView::OnContentChanged, base::Unretained(this)));
    std::unique_ptr<views::EditablePasswordCombobox> password_dropdown =
        CreateEditablePasswordCombobox(
            password_form,
            base::BindRepeating(&PasswordSaveUpdateView::TogglePasswordRevealed,
                                base::Unretained(this)));
    password_dropdown->SetCallback(base::BindRepeating(
        &PasswordSaveUpdateView::OnContentChanged, base::Unretained(this)));
    // Set up layout:
    SetLayoutManager(std::make_unique<views::FillLayout>());
    views::View* root_view = AddChildView(std::make_unique<views::View>());
    views::AnimatingLayoutManager* animating_layout =
        root_view->SetLayoutManager(
            std::make_unique<views::AnimatingLayoutManager>());
    animating_layout
        ->SetBoundsAnimationMode(views::AnimatingLayoutManager::
                                     BoundsAnimationMode::kAnimateMainAxis)
        .SetOrientation(views::LayoutOrientation::kVertical);
    views::FlexLayout* flex_layout = animating_layout->SetTargetLayoutManager(
        std::make_unique<views::FlexLayout>());
    flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                                DISTANCE_CONTROL_LIST_VERTICAL),
                            0));

    username_dropdown_ = username_dropdown.get();
    password_dropdown_ = password_dropdown.get();
    BuildCredentialRows(root_view, std::move(username_dropdown),
                        std::move(password_dropdown));

    // Only non-federated credentials bubble has a username field and can
    // change states between Save and Update. Therefore, we need to have the
    // `accessibility_alert_` to inform screen readers about that change.
    accessibility_alert_ =
        root_view->AddChildView(std::make_unique<views::View>());
    AddChildView(accessibility_alert_.get());
  }

  {
    using Controller = SaveUpdateBubbleController;
    using ControllerNotifyFn = void (Controller::*)();
    auto button_clicked = [](PasswordSaveUpdateView* dialog,
                             ControllerNotifyFn func) {
      dialog->UpdateUsernameAndPasswordInModel();
      (dialog->controller_.*func)();
    };

    SetAcceptCallbackWithClose(
        base::BindRepeating(button_clicked, base::Unretained(this),
                            &Controller::OnSaveClicked)
            .Then(base::BindRepeating(
                &PasswordSaveUpdateView::CloseOrReplaceWithPromo,
                base::Unretained(this))));
    SetCancelCallback(base::BindOnce(
        button_clicked, base::Unretained(this),
        is_update_bubble_ ? &Controller::OnNoThanksClicked
                          : &Controller::OnNeverForThisSiteClicked));
  }

  SetShowIcon(true);
  SetFootnoteView(CreateFooterView());

  UpdateBubbleUIElements();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  CHECK(hats_service);
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerAutofillPassword, web_contents, 10000);
}

PasswordSaveUpdateView::~PasswordSaveUpdateView() = default;

PasswordBubbleControllerBase* PasswordSaveUpdateView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PasswordSaveUpdateView::GetController()
    const {
  return &controller_;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
bool PasswordSaveUpdateView::OnCloseRequested(
    views::Widget::ClosedReason close_reason) {
  if (is_signin_promo_bubble_ &&
      (close_reason == views::Widget::ClosedReason::kCloseButtonClicked ||
       close_reason == views::Widget::ClosedReason::kEscKeyPressed)) {
    AutofillBubbleSignInPromoView::RecordSignInPromoDismissed(web_contents());
  }
  return true;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

bool PasswordSaveUpdateView::CloseOrReplaceWithPromo() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Close the bubble if the sign in promo should not be shown.
  if (!signin::ShouldShowPasswordSignInPromo(*controller_.GetProfile())) {
    return true;
  }

  // Remove current elements.
  username_dropdown_ = nullptr;
  password_dropdown_ = nullptr;
  accessibility_alert_ = nullptr;
  RemoveAllChildViews();
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetShowIcon(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  GetBubbleFrameView()->SetFootnoteView(nullptr);
  SetTitle(IDS_AUTOFILL_SIGNIN_PROMO_TITLE_PASSWORD);

  // Add the accessibility alert view first so that it does not overlap with
  // any other child view. Also make the view invisible.
  auto accessibility_view = std::make_unique<views::View>();
  accessibility_view->SetVisible(false);
  accessibility_alert_ = AddChildView(std::move(accessibility_view));

  auto move_callback =
      base::BindOnce(&MovePasswordToAccount, controller_.pending_password(),
                     password_manager::metrics_util::MoveToAccountStoreTrigger::
                         kUserOptedInAfterSavingLocally);

  // Show the sign in promo.
  auto sign_in_promo = std::make_unique<AutofillBubbleSignInPromoView>(
      controller_.GetWebContents(),
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
      std::move(move_callback));
  AddChildView(std::move(sign_in_promo));

  // Record that the sign in promo was shown.
  Profile* profile = controller_.GetProfile();
  AccountInfo account = signin_ui_util::GetSingleAccountForPromos(
      IdentityManagerFactory::GetForProfile(profile));

  if (account.gaia.empty()) {
    int show_count = profile->GetPrefs()->GetInteger(
        prefs::kPasswordSignInPromoShownCountPerProfile);
    profile->GetPrefs()->SetInteger(
        prefs::kPasswordSignInPromoShownCountPerProfile, show_count + 1);
  } else {
    SigninPrefs(*profile->GetPrefs())
        .IncrementPasswordSigninPromoImpressionCount(account.gaia);
  }

  // Notify the screen reader that the bubble changed.
  AnnounceBubbleChange();

  is_signin_promo_bubble_ = true;
  GetBubbleFrameView()->SetProperty(views::kElementIdentifierKey,
                                    kPasswordBubble);

  return false;
#else
  return true;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

views::View* PasswordSaveUpdateView::GetInitiallyFocusedView() {
  if (username_dropdown_ && username_dropdown_->GetText().empty()) {
    return username_dropdown_;
  }
  View* initial_view = PasswordBubbleViewBase::GetInitiallyFocusedView();
  // |initial_view| will normally be the 'Save' button, but in case it's not
  // focusable, we return nullptr so the Widget doesn't give focus to the next
  // focusable View, which would be |username_dropdown_|, and which would
  // bring up the menu without a user interaction. We only allow initial focus
  // on |username_dropdown_| above, when the text is empty.
  return (initial_view && initial_view->IsFocusable()) ? initial_view : nullptr;
}

bool PasswordSaveUpdateView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  return button != ui::mojom::DialogButton::kOk ||
         controller_.pending_password().IsFederatedCredential() ||
         !controller_.pending_password().password_value.empty();
}

ui::ImageModel PasswordSaveUpdateView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void PasswordSaveUpdateView::AddedToWidget() {
  static_cast<views::Label*>(GetBubbleFrameView()->title())
      ->SetAllowCharacterBreak(true);
  SetBubbleHeader(IDR_SAVE_PASSWORD, IDR_SAVE_PASSWORD_DARK);
}

void PasswordSaveUpdateView::UpdateUsernameAndPasswordInModel() {
  if (!username_dropdown_ && !password_dropdown_) {
    return;
  }
  std::u16string new_username = controller_.pending_password().username_value;
  std::u16string new_password = controller_.pending_password().password_value;
  if (username_dropdown_) {
    new_username = username_dropdown_->GetText();
    base::TrimString(new_username, u" ", &new_username);
  }
  if (password_dropdown_) {
    new_password = password_dropdown_->GetText();
  }
  controller_.OnCredentialEdited(std::move(new_username),
                                 std::move(new_password));
}

void PasswordSaveUpdateView::UpdateBubbleUIElements() {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  std::u16string ok_button_text;
  if (controller_.IsAccountStorageOptInRequiredBeforeSave()) {
    ok_button_text = l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_SAVE_BUBBLE_OPT_IN_BUTTON);
  } else {
    ok_button_text = l10n_util::GetStringUTF16(
        controller_.IsCurrentStateUpdate() ? IDS_PASSWORD_MANAGER_UPDATE_BUTTON
                                           : IDS_PASSWORD_MANAGER_SAVE_BUTTON);
  }
  SetButtonLabel(ui::mojom::DialogButton::kOk, ok_button_text);
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(
          is_update_bubble_ ? IDS_PASSWORD_MANAGER_CANCEL_BUTTON
                            : IDS_PASSWORD_MANAGER_BUBBLE_BLOCKLIST_BUTTON));
  // If the title is going to change, we should announce it to the screen
  // readers.
  bool should_announce_save_update_change =
      GetWindowTitle() != controller_.GetTitle();

  SetTitle(controller_.GetTitle());

  // Nothing to do if the bubble isn't visible yet.
  if (!GetWidget()) {
    return;
  }

  UpdateFootnote();

  if (should_announce_save_update_change) {
    AnnounceBubbleChange();
  }
}

std::unique_ptr<views::View> PasswordSaveUpdateView::CreateFooterView() {
  base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
      [](PasswordSaveUpdateView* dialog) {
        dialog->controller_.OnGooglePasswordManagerLinkClicked(
            password_manager::ManagePasswordsReferrer::kSaveUpdateBubble);
      },
      base::Unretained(this));
  if (controller_.IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount()) {
    return CreateGooglePasswordManagerLabel(
        /*text_message_id=*/
        IDS_PASSWORD_BUBBLES_FOOTER_SYNCED_TO_ACCOUNT,
        /*link_message_id=*/
        IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
        controller_.GetPrimaryAccountEmail(), open_password_manager_closure);
  }
  return CreateGooglePasswordManagerLabel(
      /*text_message_id=*/
      IDS_PASSWORD_BUBBLES_FOOTER_SAVING_ON_DEVICE,
      /*link_message_id=*/
      IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE,
      open_password_manager_closure);
}

void PasswordSaveUpdateView::AnnounceBubbleChange() {
  // Federated credentials bubbles don't change the state between Update and
  // Save, and hence they don't have an `accessibility_alert_` view created.
  if (!accessibility_alert_) {
    return;
  }

  views::ViewAccessibility& ax = accessibility_alert_->GetViewAccessibility();
  ax.SetRole(ax::mojom::Role::kAlert);
  ax.SetName(GetWindowTitle(), ax::mojom::NameFrom::kAttribute);
  accessibility_alert_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                                 true);
}

void PasswordSaveUpdateView::OnContentChanged() {
  bool is_update_state_before = controller_.IsCurrentStateUpdate();
  bool is_ok_button_enabled_before =
      IsDialogButtonEnabled(ui::mojom::DialogButton::kOk);
  bool changes_synced_to_account_before =
      controller_.IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount();
  UpdateUsernameAndPasswordInModel();
  // Maybe the buttons should be updated.
  if (is_update_state_before != controller_.IsCurrentStateUpdate() ||
      is_ok_button_enabled_before !=
          IsDialogButtonEnabled(ui::mojom::DialogButton::kOk)) {
    UpdateBubbleUIElements();
    DialogModelChanged();
  } else if (changes_synced_to_account_before !=
             controller_
                 .IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount()) {
    // For account store users, there is a different footnote when affecting the
    // account store.
    UpdateFootnote();
  }
}

void PasswordSaveUpdateView::UpdateFootnote() {
  DCHECK(GetBubbleFrameView());
  GetBubbleFrameView()->SetFootnoteView(CreateFooterView());
}

void PasswordSaveUpdateView::TogglePasswordRevealed() {
  if (password_dropdown_->ArePasswordsRevealed()) {
    password_dropdown_->RevealPasswords(false);
    return;
  }
  // User authentication might be required, query the controller to determine
  // whether the user is allowed to unmask the password.

  // Prevent the bubble from closing for the duration of the lifetime of the
  // `pin`. This is to keep it open while the user authentication is in action.
  std::unique_ptr<CloseOnDeactivatePin> pin = PreventCloseOnDeactivate();

  controller_.ShouldRevealPasswords(base::BindOnce(
      [](PasswordSaveUpdateView* view,
         std::unique_ptr<CloseOnDeactivatePin> pin, bool reveal) {
        view->password_dropdown_->RevealPasswords(reveal);
        // This is necessary on Windows since the bubble isn't activated again
        // after the conlusion of the auth flow.
        view->GetWidget()->Activate();
        // Delay the destruction of `pin` for 1 sec to make sure the bubble
        // remains open till the OS closes the authentication dialog and
        // reactivates the bubble.
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce([](std::unique_ptr<CloseOnDeactivatePin> pin) {},
                           std::move(pin)),
            base::Seconds(1));
      },
      base::Unretained(this), std::move(pin)));
}

BEGIN_METADATA(PasswordSaveUpdateView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PasswordSaveUpdateView, kPasswordBubble);
