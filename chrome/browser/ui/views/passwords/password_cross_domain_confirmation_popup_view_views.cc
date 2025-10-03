// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_cross_domain_confirmation_popup_view_views.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_controller_interface.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "url/gurl.h"

PasswordCrossDomainConfirmationPopupViewViews::
    PasswordCrossDomainConfirmationPopupViewViews(
        base::WeakPtr<PasswordCrossDomainConfirmationPopupControllerInterface>
            controller,
        views::Widget* parent_widget,
        const GURL& domain,
        const std::u16string& password_hostname,
        base::OnceClosure confirmation_callback,
        base::OnceClosure cancel_callback)
    : autofill::PopupBaseView(controller,
                              parent_widget,
                              views::Widget::InitParams::Activatable::kYes) {
  SetBackground(views::CreateSolidBackground(ui::kColorDropdownBackground));

  auto* layout_provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto* headline = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(layout_provider->GetDistanceMetric(
              views::DISTANCE_RELATED_LABEL_HORIZONTAL))
          .SetInsideBorderInsets(
              layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE))
          .Build());
  headline->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon,
          layout_provider->GetDistanceMetric(
              views::DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE))));
  headline->AddChildView(
      views::Builder<views::Label>()
          .SetText(controller->GetTitleText())
          .SetTextStyle(views::style::TextStyle::STYLE_BODY_3_MEDIUM)
          .SetAccessibleRole(ax::mojom::Role::kHeading)
          .Build());

  auto* body = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetInsideBorderInsets(
              layout_provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION)
                  .set_bottom(0))
          .Build());
  auto* label = body->AddChildView(
      views::Builder<views::Label>()
          .SetText(controller->GetBodyText())
          .SetMultiLine(true)
          .SetTextStyle(views::style::TextStyle::STYLE_BODY_3)
          .SetEnabledColor(ui::kColorLabelForegroundSecondary)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .Build());
  EmphasizeTokens(label, views::style::TextStyle::STYLE_BODY_3_BOLD,
                  /*tokens=*/
                  {password_hostname, base::ASCIIToUTF16(domain.GetHost())});

  auto* controls = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
          .SetInsideBorderInsets(
              layout_provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION))
          .SetBetweenChildSpacing(layout_provider->GetDistanceMetric(
              views::DISTANCE_RELATED_BUTTON_HORIZONTAL))
          .Build());
  controls->AddChildView(views::Builder<views::MdTextButton>()
                             .SetText(l10n_util::GetStringUTF16(IDS_CANCEL))
                             .SetStyle(ui::ButtonStyle::kDefault)
                             .SetCallback(std::move(cancel_callback))
                             .Build());
  auto* confirmation_button = controls->AddChildView(
      views::Builder<views::MdTextButton>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_PASSWORD_CROSS_DOMAIN_FILLING_CONFIRMATION_CONFIRM_BUTTON_LABEL))
          .SetStyle(ui::ButtonStyle::kProminent)
          .SetCallback(std::move(confirmation_callback))
          .Build());
  confirmation_button->GetViewAccessibility().SetName(base::JoinString(
      {controller->GetTitleText(), controller->GetBodyText(),
       l10n_util::GetStringUTF16(
           IDS_PASSWORD_CROSS_DOMAIN_FILLING_CONFIRMATION_CONFIRM_BUTTON_LABEL)},
      u" "));
  int popup_width = std::max(headline->GetPreferredSize().width(),
                             layout_provider->GetDistanceMetric(
                                 DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH));
  SetPreferredSize(gfx::Size(popup_width, GetHeightForWidth(popup_width)));
  SetInitiallyFocusedView(confirmation_button);
}

PasswordCrossDomainConfirmationPopupViewViews::
    ~PasswordCrossDomainConfirmationPopupViewViews() = default;

void PasswordCrossDomainConfirmationPopupViewViews::Hide() {
  weak_factory_.InvalidateWeakPtrs();

  // TODO(b/333505414): `DoHide()` must be at the end, as it does `delete this`
  // in PopupBaseView, reconsider this behaviour and remove this warning.
  DoHide();
}

bool PasswordCrossDomainConfirmationPopupViewViews::
    OverlapsWithPictureInPictureWindow() const {
  return autofill::BoundsOverlapWithPictureInPictureWindow(GetBoundsInScreen());
}

void PasswordCrossDomainConfirmationPopupViewViews::Show() {
  DoShow();
}

BEGIN_METADATA(PasswordCrossDomainConfirmationPopupViewViews)
END_METADATA

// static
base::WeakPtr<PasswordCrossDomainConfirmationPopupView>
PasswordCrossDomainConfirmationPopupView::Show(
    base::WeakPtr<PasswordCrossDomainConfirmationPopupControllerInterface>
        controller,
    const GURL& domain,
    const std::u16string& password_hostname,
    base::OnceClosure confirmation_callback,
    base::OnceClosure cancel_callback) {
  auto* view = new PasswordCrossDomainConfirmationPopupViewViews(
      controller, /*parent_widget=*/
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view()),
      domain, password_hostname, std::move(confirmation_callback),
      std::move(cancel_callback));
  view->Show();
  return view->GetWeakPtr();
}
