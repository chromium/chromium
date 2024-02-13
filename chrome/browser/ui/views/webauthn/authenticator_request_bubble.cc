// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "cc/paint/skottie_wrapper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/gfx/text_constants.h"
#include "ui/lottie/animation.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/webauthn/mac_authentication_view.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

struct BubbleContents {
  int illustration_light_id = -1;
  int buttons = ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  const char16_t* title;
  const char16_t* body;
  bool show_footer = false;
  bool close_on_deactivate = false;
  void (AuthenticatorRequestDialogModel::*on_ok)();
  void (AuthenticatorRequestDialogModel::*on_cancel)() =
      &AuthenticatorRequestDialogModel::StartOver;
};

constexpr BubbleContents kGPMTouchID = {
    .title = u"Touch ID to proceed (UNTRANSLATED)",
    .body = nullptr,
    .show_footer = false,
    .on_ok = &AuthenticatorRequestDialogModel::OnGPMCreate,
};

constexpr BubbleContents kGPMCreatePasskeyContents = {
    .illustration_light_id = IDR_WEBAUTHN_GPM_FINGERPRINT_LIGHT,
    .title = u"Create passkey for example.com? (UNTRANSLATED)",
    .body = nullptr,
    .show_footer = true,
    .on_ok = &AuthenticatorRequestDialogModel::OnGPMCreate,
};

constexpr BubbleContents kTrustThisComputerContents = {
    .illustration_light_id = IDR_WEBAUTHN_GPM_LAPTOP_LIGHT,
    .title =
        u"Trust this device to use your passkeys from Google Password Manager? "
        u"(UNTRANSLATED)",
    .body =
        u"This device will be enrolled to use your passkeys saved in Google "
        u"Password Manager. If this is a temporary device, select more "
        u"options. (UNTRANSLATED)",
    .show_footer = false,
    .on_ok = &AuthenticatorRequestDialogModel::OnTrustThisComputer,
};

constexpr BubbleContents kGPMOnboardingContents = {
    .illustration_light_id = IDR_WEBAUTHN_GPM_FINGERPRINT_LIGHT,
    .title =
        u"Start using passkeys with your Google Password Manager "
        u"(UNTRANSLATED)",
    .body =
        u"We'll create a passkey for you to sign in to example.com "
        u"(UNTRANSLATED)",
    .show_footer = true,
    .on_ok = &AuthenticatorRequestDialogModel::OnGPMOnboardingAccepted,
};

class AuthenticatorRequestBubbleDelegate
    : public views::BubbleDialogDelegate,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  AuthenticatorRequestBubbleDelegate(views::View* anchor_view,
                                     AuthenticatorRequestDialogModel* model)
      : BubbleDialogDelegate(anchor_view,
                             views::BubbleBorder::Arrow::TOP_RIGHT),
        model_(model),
        step_(model_->current_step()),
        bubble_contents_(GetContents(step_)) {
    model_->AddObserver(this);

    SetShowCloseButton(true);
    SetButtonLabel(ui::DIALOG_BUTTON_OK, u"Continue (UT)");
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, u"More options (UT)");

    SetAcceptCallbackWithClose(base::BindRepeating(
        &AuthenticatorRequestBubbleDelegate::OnOk, base::Unretained(this)));
    SetCancelCallbackWithClose(base::BindRepeating(
        &AuthenticatorRequestBubbleDelegate::OnCancel, base::Unretained(this)));

    set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
    set_corner_radius(16);

    std::unique_ptr<views::View> primary_view =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .Build();
    primary_view_ = primary_view.get();
    ConfigureView(model_->current_step());

    SetContentsView(std::move(primary_view));
  }

  ~AuthenticatorRequestBubbleDelegate() override {
    if (model_) {
      model_->RemoveObserver(this);
    }
  }

 protected:
  static const BubbleContents* GetContents(
      AuthenticatorRequestDialogModel::Step step) {
    switch (step) {
      case AuthenticatorRequestDialogModel::Step::kGPMCreatePasskey:
        return &kGPMCreatePasskeyContents;
      case AuthenticatorRequestDialogModel::Step::kTrustThisComputer:
        return &kTrustThisComputerContents;
      case AuthenticatorRequestDialogModel::Step::kGPMTouchID:
        return &kGPMTouchID;
      case AuthenticatorRequestDialogModel::Step::kGPMOnboarding:
        return &kGPMOnboardingContents;
      default:
        NOTREACHED();
        return nullptr;
    }
  }

  static std::unique_ptr<views::View> CreateViewForContents(
      const BubbleContents& contents) {
    std::unique_ptr<views::View> vbox =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .Build();

    if (contents.illustration_light_id >= 0) {
      // TODO: also need dark-mode illustrations when those assets are
      // available.
      std::optional<std::vector<uint8_t>> lottie_bytes =
          ui::ResourceBundle::GetSharedInstance().GetLottieData(
              contents.illustration_light_id);
      scoped_refptr<cc::SkottieWrapper> skottie =
          cc::SkottieWrapper::CreateSerializable(std::move(*lottie_bytes));
      auto animation = std::make_unique<views::AnimatedImageView>();
      animation->SetPreferredSize(gfx::Size(320, 106));
      animation->SetAnimatedImage(std::make_unique<lottie::Animation>(skottie));
      animation->SizeToPreferredSize();
      animation->Play();
      vbox->AddChildView(animation.release());
    }

    vbox->AddChildView(views::Builder<views::StyledLabel>()
                           .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                           .SetTextContext(views::style::STYLE_PRIMARY)
                           .SetText(contents.title)
                           .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
                           .Build());

    if (contents.body) {
      vbox->AddChildView(
          views::Builder<views::StyledLabel>()
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetTextContext(views::style::STYLE_PRIMARY)
              .SetText(contents.body)
              .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
              .Build());
    }

    return vbox;
  }

  void ConfigureView(AuthenticatorRequestDialogModel::Step step) {
    set_close_on_deactivate(bubble_contents_->close_on_deactivate);
    SetButtons(bubble_contents_->buttons);
    if (bubble_contents_->show_footer) {
      auto* label = SetFootnoteView(std::make_unique<views::Label>(
          u"Your passkeys are saved to Google Password Manager for "
          u"example@gmail.com and will also be available on your Android "
          u"devices (UNTRANSLATED)",
          ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
          views::style::STYLE_SECONDARY));
      label->SetMultiLine(true);
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    } else {
      SetFootnoteView(std::unique_ptr<views::View>());
    }

    std::unique_ptr<views::View> view =
        CreateViewForContents(*bubble_contents_);
    primary_view_->AddChildView(std::move(view));
#if BUILDFLAG(IS_MAC)
    if (step == AuthenticatorRequestDialogModel::Step::kGPMTouchID) {
      if (__builtin_available(macos 12, *)) {
        primary_view_->AddChildView(
            std::make_unique<MacAuthenticationView>(base::DoNothing()));
      }
    }
#endif  // BUILDFLAG(IS_MAC)
  }

  bool OnOk() {
    if (bubble_contents_) {
      (model_->*(bubble_contents_->on_ok))();
    }
    return false;  // don't close this bubble.
  }

  bool OnCancel() {
    if (bubble_contents_) {
      (model_->*(bubble_contents_->on_cancel))();
    }
    return false;  // don't close this bubble.
  }

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    model_ = nullptr;
  }

  void OnStepTransition() override {
    // The bubble is destroyed and recreated for each step because updating the
    // footnote view doesn't appear to work.
    if (model_->current_step() != step_) {
      GetWidget()->Close();
      // TODO: create a new bubble for the new step. Not done until it can
      // be tested in practice.
      return;
    }
  }

  void OnSheetModelChanged() override {}

 private:
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  const AuthenticatorRequestDialogModel::Step step_;
  const raw_ptr<const BubbleContents> bubble_contents_;
  raw_ptr<views::View> primary_view_;
};

}  // namespace

void ShowAuthenticatorRequestBubble(content::WebContents* web_contents,
                                    AuthenticatorRequestDialogModel* model) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  browser->window()->UpdatePageActionIcon(PageActionIconType::kManagePasswords);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ToolbarButtonProvider* button_provider =
      browser_view->toolbar_button_provider();
  views::View* anchor_view =
      button_provider->GetAnchorView(PageActionIconType::kManagePasswords);
  auto bubble =
      std::make_unique<AuthenticatorRequestBubbleDelegate>(anchor_view, model);
  // Showing the GPM icon is possible with the following but we would need to
  // update the passwords UI logic because it will currently CHECK if you click
  // on this icon when it doesn't think that it should be showing.
  // TODO: decide if we want to show the icon.
  //
  // button_provider->GetPageActionIconView(PageActionIconType::kManagePasswords)
  //     ->SetVisible(true);
  // bubble->SetHighlightedButton(button_provider->GetPageActionIconView(
  //     PageActionIconType::kManagePasswords));
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  widget->Show();
}
