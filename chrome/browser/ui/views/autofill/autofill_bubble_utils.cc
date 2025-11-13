// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_bubble_utils.h"

#include <memory>
#include <utility>

#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill {
namespace {

constexpr int kIconSize = 16;
constexpr int kWalletIconSize = 20;
}  // namespace

std::unique_ptr<views::ImageButton> CreateEditButton(
    views::Button::PressedCallback callback) {
  std::unique_ptr<views::ImageButton> button =
      views::CreateVectorImageButtonWithNativeTheme(
          std::move(callback), vector_icons::kEditIcon, kIconSize);
  button->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
  InstallCircleHighlightPathGenerator(button.get());
  return button;
}

ui::ImageModel CreateWalletIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return ui::ImageModel::FromVectorIcon(vector_icons::kGoogleWalletIcon,
                                        ui::kColorIcon, kWalletIconSize);

#else
  // This is a placeholder icon on non-branded builds.
  return ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon,
                                        ui::kColorIcon, kWalletIconSize);
#endif
}

std::unique_ptr<views::View> CreateWalletBubbleTitleView(
    const std::u16string& title) {
  auto title_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .Build();

  auto* label = title_view->AddChildView(
      views::Builder<views::Label>()
          .SetText(title)
          .SetTextStyle(views::style::STYLE_HEADLINE_4)
          .SetMultiLine(true)
          .SetAccessibleRole(ax::mojom::Role::kTitleBar)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .Build());

  title_view->AddChildView(
      views::Builder<views::ImageView>().SetImage(CreateWalletIcon()).Build());
  title_view->SetFlexForView(label, 1);
  return title_view;
}

}  // namespace autofill
