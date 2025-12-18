// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_sign_in_toolbar.h"

#include <utility>

#include "base/check.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// Padding of elements in the simple toolbar, matching the padding in profile
// picker type choice screen.
constexpr gfx::Insets kToolbarPadding = gfx::Insets(8).set_left(16);

class SimpleBackButton : public ToolbarButton {
  METADATA_HEADER(SimpleBackButton, ToolbarButton)

 public:
  explicit SimpleBackButton(PressedCallback callback)
      : ToolbarButton(std::move(callback)) {
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    SetVectorIcons(vector_icons::kBackArrowIcon, kBackArrowTouchIcon);
    SetTooltipText(l10n_util::GetStringUTF16(
        IDS_PROFILE_PICKER_BACK_BUTTON_SIGN_IN_LABEL));
    // Unlike toolbar buttons, this one should be focusable to make it
    // consistent with other screens of the flow where the back button is part
    // of the page.
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }
  SimpleBackButton(const SimpleBackButton&) = delete;
  SimpleBackButton& operator=(const SimpleBackButton&) = delete;
  ~SimpleBackButton() override = default;
};

BEGIN_METADATA(SimpleBackButton)
END_METADATA

}  // namespace

ProfilePickerSignInToolbar::ProfilePickerSignInToolbar() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetInteriorMargin(kToolbarPadding);
  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  SetLayoutManagerUseConstrainedSpace(false);
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                       views::MaximumFlexSizeRule::kPreferred));
}

ProfilePickerSignInToolbar::~ProfilePickerSignInToolbar() = default;

void ProfilePickerSignInToolbar::BuildToolbar(
    base::RepeatingClosure on_back_callback) {
  DCHECK(children().empty());
  // Create the toolbar back button.
  auto back_button =
      std::make_unique<SimpleBackButton>(std::move(on_back_callback));
  AddChildView(std::move(back_button));

  // Set the background to transparent to inherit the color from sign-in page.
  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  // Ensure toolbar is drawn on top of the WebView. Mark it as non-opaque to let
  // the web content show through the transparent background.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

BEGIN_METADATA(ProfilePickerSignInToolbar)
END_METADATA
