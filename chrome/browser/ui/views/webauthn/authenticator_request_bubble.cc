// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "cc/paint/skottie_wrapper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/webauthn/authenticator_request_bubble.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/lottie/animation.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {

struct BubbleContents {
  int illustration_light_id = -1;
  int buttons = ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  const char16_t* title;
  const char16_t* body;
  bool show_footer = false;
  bool show_icon = false;
  bool close_on_deactivate = false;
  void (AuthenticatorRequestDialogModel::*on_ok)();
  void (AuthenticatorRequestDialogModel::*on_cancel)() =
      &AuthenticatorRequestDialogModel::StartOver;
};

// TODO(rgod): Add username row and correct footer when mocks are ready.
constexpr BubbleContents kGPMPasskeySavedContents = {
    .buttons = ui::DIALOG_BUTTON_NONE,
    .title = u"Passkey saved (UT)",
    .show_footer = true,
    .show_icon = true,
    .on_cancel = &AuthenticatorRequestDialogModel::OnRequestComplete,
};

class AuthenticatorRequestBubbleDelegate
    : public views::BubbleDialogDelegateView,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  AuthenticatorRequestBubbleDelegate(views::View* anchor_view,
                                     AuthenticatorRequestDialogModel* model)
      : BubbleDialogDelegateView(anchor_view,
                                 views::BubbleBorder::Arrow::TOP_RIGHT),
        model_(model),
        step_(model_->step()),
        bubble_contents_(GetContents(step_)) {
    model_->observers.AddObserver(this);

    SetShowCloseButton(true);
    SetButtonLabel(ui::DIALOG_BUTTON_OK, u"Continue (UT)");
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, u"More options (UT)");

    SetAcceptCallbackWithClose(base::BindRepeating(
        &AuthenticatorRequestBubbleDelegate::OnOk, base::Unretained(this)));
    SetCancelCallbackWithClose(base::BindRepeating(
        &AuthenticatorRequestBubbleDelegate::OnCancel, base::Unretained(this)));

    SetIcon(ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                           ui::kColorIcon));

    set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
    set_corner_radius(16);

    SetLayoutManager(std::make_unique<views::FillLayout>());
    ConfigureView();
  }

  ~AuthenticatorRequestBubbleDelegate() override {
    if (model_) {
      model_->observers.RemoveObserver(this);
    }
  }

 protected:
  static const BubbleContents* GetContents(
      AuthenticatorRequestDialogModel::Step step) {
    switch (step) {
      case AuthenticatorRequestDialogModel::Step::kGPMPasskeySaved:
        return &kGPMPasskeySavedContents;
      default:
        NOTREACHED();
        return nullptr;
    }
  }

  void ConfigureView() {
    UpdateHeader();
    UpdateFootnote();
    UpdateBody();

    set_close_on_deactivate(bubble_contents_->close_on_deactivate);
    SetButtons(bubble_contents_->buttons);
    SetTitle(bubble_contents_->title);
    SetShowIcon(bubble_contents_->show_icon);
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
    if (model_->should_bubble_be_closed()) {
      if (GetWidget()) {
        GetWidget()->Close();
      }
      return;
    }

    if (model_->step() == step_) {
      return;
    }

    step_ = model_->step();
    bubble_contents_ = GetContents(step_);
    ConfigureView();
    SizeToContents();
  }

 private:
  void UpdateHeader() {
    if (!GetWidget()) {
      return;
    }

    // TODO(rgod): also need dark-mode illustrations when those assets are
    // available.
    if (bubble_contents_->illustration_light_id >= 0) {
      std::optional<std::vector<uint8_t>> lottie_bytes =
          ui::ResourceBundle::GetSharedInstance().GetLottieData(
              bubble_contents_->illustration_light_id);
      scoped_refptr<cc::SkottieWrapper> skottie =
          cc::SkottieWrapper::UnsafeCreateSerializable(
              std::move(*lottie_bytes));
      auto animation = std::make_unique<views::AnimatedImageView>();
      animation->SetPreferredSize(gfx::Size(320, 106));
      animation->SetAnimatedImage(std::make_unique<lottie::Animation>(skottie));
      animation->SizeToPreferredSize();
      animation->Play();
      GetBubbleFrameView()->SetHeaderView(std::move(animation));
    } else {
      GetBubbleFrameView()->SetHeaderView(nullptr);
    }
  }

  void UpdateFootnote() {
    if (!GetWidget()) {
      return;
    }

    if (bubble_contents_->show_footer) {
      auto label = std::make_unique<views::Label>(
          u"Your passkeys are saved to Google Password Manager for "
          u"example@gmail.com and will also be available on your Android "
          u"devices (UNTRANSLATED)",
          ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
          views::style::STYLE_SECONDARY);
      label->SetMultiLine(true);
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      GetBubbleFrameView()->SetFootnoteView(std::move(label));
    } else {
      GetBubbleFrameView()->SetFootnoteView(nullptr);
    }
  }

  void UpdateBody() {
    // Remove the view if it exists and re-create it. This is necessary during
    // the `step_` transition.
    if (primary_view_) {
      RemoveChildView(primary_view_.get());
    }

    std::unique_ptr<views::View> primary_view =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .Build();
    primary_view_ = primary_view.get();

    if (bubble_contents_->body) {
      primary_view_->AddChildView(
          views::Builder<views::StyledLabel>()
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetText(bubble_contents_->body)
              .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
              .Build());
    }
    AddChildView(std::move(primary_view));
  }

  // views::View:
  void AddedToWidget() override {
    UpdateHeader();
    UpdateFootnote();
  }
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  AuthenticatorRequestDialogModel::Step step_;
  raw_ptr<const BubbleContents> bubble_contents_;
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
