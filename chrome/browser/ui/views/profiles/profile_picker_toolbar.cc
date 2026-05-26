// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_toolbar.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kProfilePickerToolbarDontSignInButtonElementId);

namespace {

// Padding of elements in the simple toolbar, matching the padding in profile
// picker type choice screen.
constexpr gfx::Insets kToolbarPadding =
    gfx::Insets(8).set_left(16).set_right(16);

class SimpleBackButton : public ToolbarButton {
  METADATA_HEADER(SimpleBackButton, ToolbarButton)

 public:
  explicit SimpleBackButton(PressedCallback callback)
      : ToolbarButton(std::move(callback)) {
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    SetVectorIcons(features::IsRoundedIconsEnabled()
                       ? vector_icons::kArrowBackIcon
                       : vector_icons::kBackArrowOldIcon,
                   features::IsRoundedIconsEnabled() ? kArrowBackIcon
                                                     : kBackArrowTouchOldIcon);
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

class DontSignInButton : public ToolbarButton {
  METADATA_HEADER(DontSignInButton, ToolbarButton)

 public:
  explicit DontSignInButton(PressedCallback callback)
      : ToolbarButton(std::move(callback)) {
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    SetHighlight(l10n_util::GetStringUTF16(
                     IDS_FRE_NATIVE_TOOLBAR_DONT_SIGN_IN_BUTTON_LABEL),
                 SK_ColorTRANSPARENT);
    // Unlike usual toolbar buttons, this one should be focusable to make it
    // consistent with the first screen of the flow where the "Don't sign in"
    // button is part of the page.
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetProperty(views::kElementIdentifierKey,
                kProfilePickerToolbarDontSignInButtonElementId);
  }

  DontSignInButton(const DontSignInButton&) = delete;
  DontSignInButton& operator=(const DontSignInButton&) = delete;

  ~DontSignInButton() override = default;

  std::optional<SkColor> GetHighlightBorderColor() const override {
    const auto* const color_provider = GetColorProvider();
    CHECK(color_provider);
    return color_provider->GetColor(ui::kColorSysPrimary);
  }

  std::optional<SkColor> GetHighlightTextColor() const override {
    const auto* const color_provider = GetColorProvider();
    CHECK(color_provider);
    return color_provider->GetColor(ui::kColorSysPrimary);
  }

  bool ShouldBlendHighlightColor() const override { return false; }

  void UpdateColorsAndInsets() override {
    ToolbarButton::UpdateColorsAndInsets();
    // ToolbarButton adds spacing to one side of the label to separate it from
    // the (missing) icon. Remove this to center the text properly.
    // Also add some padding on both sides.
    label()->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 8)));
  }
};

BEGIN_METADATA(DontSignInButton)
END_METADATA

}  // namespace

ProfilePickerToolbar::Builder::Builder(base::RepeatingClosure on_back_callback)
    : on_back_callback_(std::move(on_back_callback)) {}

ProfilePickerToolbar::Builder::~Builder() = default;

ProfilePickerToolbar::Builder&
ProfilePickerToolbar::Builder::WithDontSignInButton(
    base::RepeatingClosure on_dont_sign_in_callback) {
  on_dont_sign_in_callback_ = std::move(on_dont_sign_in_callback);
  return *this;
}

std::unique_ptr<ProfilePickerToolbar> ProfilePickerToolbar::Builder::Build() {
  auto toolbar = base::WrapUnique(new ProfilePickerToolbar());
  toolbar->AddBackButton(on_back_callback_);
  if (!on_dont_sign_in_callback_.is_null()) {
    // Add a spacer to push the subsequent button to the other side.
    toolbar->AddSpacer();
    toolbar->AddDontSignInButton(on_dont_sign_in_callback_);
  }
  return toolbar;
}

ProfilePickerToolbar::ProfilePickerToolbar() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetInteriorMargin(kToolbarPadding);
  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  SetLayoutManagerUseConstrainedSpace(false);
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                       views::MaximumFlexSizeRule::kUnbounded)
                  .WithWeight(1));

  // Set the background to transparent to inherit the color from sign-in page.
  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  // Ensure toolbar is drawn on top of the WebView. Mark it as non-opaque to let
  // the web content show through the transparent background.
  SetPaintToLayer();
  CHECK_DEREF(layer()).SetFillsBoundsOpaquely(false);
}

ProfilePickerToolbar::~ProfilePickerToolbar() = default;

void ProfilePickerToolbar::AddSpacer() {
  auto spacer = std::make_unique<views::View>();
  spacer->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));
  AddChildView(std::move(spacer));
}

void ProfilePickerToolbar::AddBackButton(
    base::RepeatingClosure on_back_callback) {
  CHECK(sign_in_back_button_ == nullptr);
  CHECK(!on_back_callback.is_null());
  sign_in_back_button_ = AddChildView(
      std::make_unique<SimpleBackButton>(std::move(on_back_callback)));
  sign_in_back_button_->SetVisible(false);
}

void ProfilePickerToolbar::AddDontSignInButton(
    base::RepeatingClosure on_dont_sign_in_callback) {
  CHECK(dont_sign_in_button_ == nullptr);
  CHECK(!on_dont_sign_in_callback.is_null());
  dont_sign_in_button_ = AddChildView(
      std::make_unique<DontSignInButton>(std::move(on_dont_sign_in_callback)));
  dont_sign_in_button_->SetVisible(false);
}

void ProfilePickerToolbar::SetSigninButtonsVisible(bool visible) {
  SetBackButtonVisible(visible);
  SetDontSignInButtonVisible(visible);
}

bool ProfilePickerToolbar::AreSigninButtonsVisibleForTesting() const {
  if (!GetVisible()) {
    // If the toolbar itself is not visible, then no buttons are visible.
    return false;
  }
  return CHECK_DEREF(sign_in_back_button_).GetVisible() ||
         (dont_sign_in_button_ && dont_sign_in_button_->GetVisible());
}

void ProfilePickerToolbar::SetDontSignInButtonVisible(bool visible) {
  if (dont_sign_in_button_) {
    dont_sign_in_button_->SetVisible(visible);
  }
}

void ProfilePickerToolbar::SetBackButtonVisible(bool visible) {
  // The back button should always be created.
  CHECK(sign_in_back_button_);
  sign_in_back_button_->SetVisible(visible);
}

BEGIN_METADATA(ProfilePickerToolbar)
END_METADATA
