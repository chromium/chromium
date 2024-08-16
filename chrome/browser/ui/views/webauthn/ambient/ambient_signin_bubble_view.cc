// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/ambient/ambient_signin_bubble_view.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

struct AuthenticatorRequestDialogModel;

using content::WebContents;

namespace ambient_signin {

inline constexpr int kRightMargin = 40;
inline constexpr int kTopMargin = 16;

BEGIN_METADATA(AmbientSigninBubbleView)
END_METADATA

AmbientSigninBubbleView::AmbientSigninBubbleView(
    WebContents* web_contents,
    View* anchor_view,
    AmbientSigninController* controller,
    AuthenticatorRequestDialogModel* model)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      web_contents_(web_contents),
      controller_(controller) {
  set_fixed_width(375);
  set_close_on_deactivate(false);
  SetShowTitle(true);
  SetTitle(u"Ambient Signin Prototype");
  SetButtons(ui::DIALOG_BUTTON_NONE);

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(views::LayoutAlignment::kStart);
  SetLayoutManager(std::move(layout));

  for (const auto& cred : model->creds) {
    auto label = std::make_unique<views::Label>(
        base::UTF8ToUTF16(cred.user.name.value_or("")));
    AddChildView(label.get());
    labels_.push_back(std::move(label));
  }
}

AmbientSigninBubbleView::~AmbientSigninBubbleView() = default;

void AmbientSigninBubbleView::Show() {
  if (!widget_) {
    widget_ = BubbleDialogDelegateView::CreateBubble(this)->GetWeakPtr();
    widget_->AddObserver(controller_);
  }
  widget_->Show();
}

void AmbientSigninBubbleView::Update() {
  NOTIMPLEMENTED();
}

void AmbientSigninBubbleView::Hide() {
  if (!widget_) {
    return;
  }
  widget_->Hide();
}

void AmbientSigninBubbleView::Close() {
  widget_->CloseNow();
}

void AmbientSigninBubbleView::NotifyWidgetDestroyed() {
  widget_->RemoveObserver(controller_);
  BubbleDialogDelegateView::OnWidgetDestroying(widget_.get());
}

// The implementation below is heavily influenced by AccountSelectionBubbleView
gfx::Rect AmbientSigninBubbleView::GetBubbleBounds() {
  CHECK(web_contents_);

  gfx::Rect bubble_bounds = BubbleDialogDelegateView::GetBubbleBounds();
  gfx::Rect web_contents_bounds = web_contents_->GetViewBounds();
  if (base::i18n::IsRTL()) {
    web_contents_bounds.Inset(
        gfx::Insets::TLBR(kTopMargin, kRightMargin, 0, 0));
    bubble_bounds.set_origin(web_contents_->GetViewBounds().origin());
  } else {
    web_contents_bounds.Inset(
        gfx::Insets::TLBR(kTopMargin, 0, 0, kRightMargin));
    bubble_bounds.set_origin(web_contents_->GetViewBounds().top_right());
  }
  bubble_bounds.AdjustToFit(web_contents_bounds);

  return bubble_bounds;
}

}  // namespace ambient_signin
