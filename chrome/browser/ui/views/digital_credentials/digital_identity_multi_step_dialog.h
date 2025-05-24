// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_MULTI_STEP_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_MULTI_STEP_DIALOG_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/models/dialog_model.h"
#include "ui/color/color_variant.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"

class DigitalIdentityMultiStepDialogDelegate;

namespace content {
class WebContents;
}

namespace views {
class BubbleDialogDelegate;
}

// Wraps views::BubbleDialogDelegate where contents can be updated in order to
// support having multiple steps in dialog.
class DigitalIdentityMultiStepDialog {
 public:
  // Configures the `illustration` to be ready for displaying in the dialog. It
  // adjusts the size and wraps it in another view. Controllers for different
  // steps in the flow will use this method to configure the coressponding
  // illustration in each step. Implementation is in the header file because
  // this is a templated method.
  template <typename T>
  static std::unique_ptr<views::BoxLayoutView> ConfigureHeaderIllustration(
      std::unique_ptr<T> illustration) {
    constexpr int kImageMarginTop = 22;
    constexpr int kImageMarginBottom = 2;
    constexpr int kImageHeight = 112;
    constexpr int kHeaderHeight =
        kImageHeight + kImageMarginTop + kImageMarginBottom;

    const gfx::Insets& insets =
        ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
            views::DialogContentType::kText, views::DialogContentType::kText);
    const int available_width =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
        insets.right() - insets.left();
    const gfx::Size header_size(available_width, kHeaderHeight);
    // `illustration` will horizontally center if the width is
    // larger than the size from the Lottie file, but the height is just used to
    // truncate the image, so that is disabled with a very large value.
    illustration->SetPreferredSize(gfx::Size(available_width, 9999));
    illustration->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kImageMarginTop, 0, kImageMarginBottom, 0)));
    illustration->SetSize(header_size);
    illustration->SetVerticalAlignment(views::ImageView::Alignment::kLeading);

    auto container_view =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetInsideBorderInsets(gfx::Insets())
            .SetPreferredSize(header_size)
            .Build();
    container_view->AddChildView(std::move(illustration));
    return container_view;
  }

  class TestApi {
   public:
    explicit TestApi(DigitalIdentityMultiStepDialog* dialog);
    ~TestApi();

    views::Widget* GetWidget();
    views::BubbleDialogDelegate* GetWidgetDelegate();

   private:
    const raw_ptr<DigitalIdentityMultiStepDialog> dialog_;
  };

  explicit DigitalIdentityMultiStepDialog(
      base::WeakPtr<content::WebContents> web_contents);
  ~DigitalIdentityMultiStepDialog();

  // Tries to show dialog. Updates the dialog contents if the dialog is already
  // showing. Runs `cancel_callback` if dialog could not be shown (and the
  // dialog is not already showing).
  void TryShow(
      const std::optional<ui::DialogModel::Button::Params>& accept_button,
      base::OnceClosure accept_callback,
      const ui::DialogModel::Button::Params& cancel_button,
      base::OnceClosure cancel_callback,
      const std::u16string& dialog_title,
      const std::u16string& body_text,
      std::unique_ptr<views::View> custom_body_field);

  ui::ColorVariant GetBackgroundColor();

 private:
  DigitalIdentityMultiStepDialogDelegate* GetWidgetDelegate();

  // The web contents the dialog is modal to.
  base::WeakPtr<content::WebContents> web_contents_;

  base::WeakPtr<views::Widget> dialog_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_MULTI_STEP_DIALOG_H_
