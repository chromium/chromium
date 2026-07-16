// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_error_bubble_view.h"

#include <stddef.h>

#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/elevation_icon_setter.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/base/buildflags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

// GlobalErrorBubbleViewBase ---------------------------------------------------

// static
GlobalErrorBubbleViewBase* GlobalErrorBubbleViewBase::ShowStandardBubbleView(
    Browser* browser,
    const base::WeakPtr<GlobalErrorWithStandardBubble>& error) {
  auto* control = BrowserView::GetBrowserViewForBrowser(browser)
                      ->toolbar_button_provider()
                      ->GetAppMenuControl();
  views::BubbleAnchor anchor =
      control ? control->GetAnchor() : views::BubbleAnchor();
  GlobalErrorBubbleView* bubble_view = new GlobalErrorBubbleView(
      anchor, views::BubbleBorder::TOP_RIGHT, browser, error);
  views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  bubble_view->GetWidget()->Show();
  return bubble_view;
}

// GlobalErrorBubbleView -------------------------------------------------------

GlobalErrorBubbleView::GlobalErrorBubbleView(
    views::BubbleAnchor anchor,
    views::BubbleBorder::Arrow arrow,
    Browser* browser,
    const base::WeakPtr<GlobalErrorWithStandardBubble>& error)
    : BubbleDialogDelegateView(anchor,
                               arrow,
                               views::BubbleBorder::DIALOG_SHADOW,
                               /*autosize=*/true),
      error_(error) {
  // error_ is a WeakPtr, but it's always non-null during construction.
  DCHECK(error_);

  WidgetDelegate::SetTitle(error_->GetBubbleViewTitle());
  WidgetDelegate::SetShowCloseButton(error_->ShouldShowCloseButton());
  WidgetDelegate::RegisterWindowClosingCallback(base::BindOnce(
      [](base::WeakPtr<GlobalErrorWithStandardBubble> error,
         base::WeakPtr<Browser> browser) {
        if (error) {
          // The browser may have been destroyed by the time the bubble closes,
          // so `browser` can be null. Call `BubbleViewDidClose` regardless
          // so the error can clear its `bubble_view_` pointer.
          // This is different from the button callbacks below, which require a
          // valid browser to perform their actions.
          error->BubbleViewDidClose(browser.get());
        }
      },
      error_, browser->AsWeakPtr()));

  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtons(!error_->GetBubbleViewCancelButtonLabel().empty()
                 ? static_cast<int>(ui::mojom::DialogButton::kCancel) |
                       static_cast<int>(ui::mojom::DialogButton::kOk)
                 : static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 error_->GetBubbleViewAcceptButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 error_->GetBubbleViewCancelButtonLabel());

  // Note that since `error` is a WeakPtr, the lambdas check if it is valid,
  // so these callbacks will do nothing if they are invoked after its
  // destruction.
  SetAcceptCallback(base::BindOnce(
      [](base::WeakPtr<GlobalErrorWithStandardBubble> error,
         base::WeakPtr<Browser> browser) {
        if (error && browser) {
          error->BubbleViewAcceptButtonPressed(browser.get());
        }
      },
      error, browser->AsWeakPtr()));
  SetCancelCallback(base::BindOnce(
      [](base::WeakPtr<GlobalErrorWithStandardBubble> error,
         base::WeakPtr<Browser> browser) {
        if (error && browser) {
          error->BubbleViewCancelButtonPressed(browser.get());
        }
      },
      error, browser->AsWeakPtr()));

  if (!error_->GetBubbleViewDetailsButtonLabel().empty()) {
    SetExtraView(std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            [](base::WeakPtr<GlobalErrorWithStandardBubble> error,
               base::WeakPtr<Browser> browser) {
              if (error && browser) {
                error->BubbleViewDetailsButtonPressed(browser.get());
              }
            },
            error_, browser->AsWeakPtr()),
        error_->GetBubbleViewDetailsButtonLabel()));
  }
}

GlobalErrorBubbleView::~GlobalErrorBubbleView() = default;

void GlobalErrorBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  for (const auto& message_string : error_->GetBubbleViewMessages()) {
    auto* message_label =
        AddChildView(std::make_unique<views::Label>(message_string));
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_label->SetMaximumWidth(362);
  }

  // These bubbles show at times where activation is sporadic (like at startup,
  // or a new window opening). Make sure the bubble doesn't disappear before the
  // user sees it, if the bubble needs to be acknowledged.
  // |error_| is assumed to be valid, and stay valid, at least until Init()
  // returns.
  set_close_on_deactivate(error_->ShouldCloseOnDeactivate());
}

void GlobalErrorBubbleView::OnWidgetInitialized() {
  views::LabelButton* ok_button = GetOkButton();
  if (ok_button && error_ && error_->ShouldAddElevationIconToAcceptButton()) {
    elevation_icon_setter_ = std::make_unique<ElevationIconSetter>(ok_button);
  }
}

void GlobalErrorBubbleView::CloseBubbleView() {
  GetWidget()->Close();
}

BEGIN_METADATA(GlobalErrorBubbleView)
END_METADATA
