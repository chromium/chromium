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
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kProfilePickerToolbarDontSignInButtonElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(
    kProfilePickerToolbarEffectsControlButtonElementId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(
    kProfilePickerToolbarStartBrowsingButtonElementId);

namespace {

// Padding of elements in the simple toolbar, matching the padding in profile
// picker type choice screen.
constexpr gfx::Insets kToolbarPadding =
    gfx::Insets(8).set_left(16).set_right(16);

// Base class for all buttons in the toolbar.
class ProfilePickerToolbarButton : public ToolbarButton {
  METADATA_HEADER(ProfilePickerToolbarButton, ToolbarButton)

 public:
  explicit ProfilePickerToolbarButton(PressedCallback callback)
      : ToolbarButton(std::move(callback)) {
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);
    // Unlike usual toolbar buttons, these should be focusable to make them
    // consistent with other screens of the flow where the button is part of the
    // page.
    SetFocusBehavior(FocusBehavior::ALWAYS);

    // Ensure the button's layer is a non-opaque so it shows through the
    // transparent background.
    SetPaintToLayer();
    CHECK_DEREF(layer()).SetFillsBoundsOpaquely(false);
  }

  ProfilePickerToolbarButton(const ProfilePickerToolbarButton&) = delete;
  ProfilePickerToolbarButton& operator=(const ProfilePickerToolbarButton&) =
      delete;

  ~ProfilePickerToolbarButton() override = default;

  // ToolbarButton:
  SkColor GetForegroundColor(ButtonState state) const override {
    const auto* color_provider = GetColorProvider();
    if (!color_provider) {
      return gfx::kPlaceholderColor;
    }
    switch (state) {
      case ButtonState::STATE_HOVERED:
      case ButtonState::STATE_PRESSED:
      case ButtonState::STATE_NORMAL:
        return color_provider->GetColor(ui::kColorSysPrimary);
      case ButtonState::STATE_DISABLED:
        return color_provider->GetColor(ui::kColorSysStateDisabled);
      default:
        NOTREACHED();
    }
  }
};

BEGIN_METADATA(ProfilePickerToolbarButton)
END_METADATA

// Base class for text based toolbar buttons.
class ProfilePickerTextToolbarButton : public ProfilePickerToolbarButton {
  METADATA_HEADER(ProfilePickerTextToolbarButton, ProfilePickerToolbarButton)

 public:
  explicit ProfilePickerTextToolbarButton(PressedCallback callback,
                                          const std::u16string& text)
      : ProfilePickerToolbarButton(std::move(callback)) {
    SetHighlight(text, SK_ColorTRANSPARENT);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
  }

  ProfilePickerTextToolbarButton(const ProfilePickerTextToolbarButton&) =
      delete;
  ProfilePickerTextToolbarButton& operator=(
      const ProfilePickerTextToolbarButton&) = delete;

  ~ProfilePickerTextToolbarButton() override = default;

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

BEGIN_METADATA(ProfilePickerTextToolbarButton)
END_METADATA

class SignInBackButton : public ProfilePickerToolbarButton {
  METADATA_HEADER(SignInBackButton, ProfilePickerToolbarButton)

 public:
  explicit SignInBackButton(PressedCallback callback)
      : ProfilePickerToolbarButton(std::move(callback)) {
    SetVectorIcons(features::IsRoundedIconsEnabled()
                       ? vector_icons::kArrowBackIcon
                       : vector_icons::kBackArrowOldIcon,
                   features::IsRoundedIconsEnabled() ? kArrowBackIcon
                                                     : kBackArrowTouchOldIcon);
    SetTooltipText(l10n_util::GetStringUTF16(
        IDS_PROFILE_PICKER_BACK_BUTTON_SIGN_IN_LABEL));
  }

  SignInBackButton(const SignInBackButton&) = delete;
  SignInBackButton& operator=(const SignInBackButton&) = delete;

  ~SignInBackButton() override = default;
};

BEGIN_METADATA(SignInBackButton)
END_METADATA

class DontSignInButton : public ProfilePickerTextToolbarButton {
  METADATA_HEADER(DontSignInButton, ProfilePickerTextToolbarButton)

 public:
  explicit DontSignInButton(PressedCallback callback, bool paint_border)
      : ProfilePickerTextToolbarButton(
            std::move(callback),
            l10n_util::GetStringUTF16(
                IDS_FRE_NATIVE_TOOLBAR_DONT_SIGN_IN_BUTTON_LABEL)),
        paint_border_(paint_border) {
    SetProperty(views::kElementIdentifierKey,
                kProfilePickerToolbarDontSignInButtonElementId);
  }

  DontSignInButton(const DontSignInButton&) = delete;
  DontSignInButton& operator=(const DontSignInButton&) = delete;

  ~DontSignInButton() override = default;

  std::optional<SkColor> GetHighlightBorderColor() const override {
    if (!paint_border_) {
      return std::nullopt;
    }
    const auto* const color_provider = GetColorProvider();
    CHECK(color_provider);
    return color_provider->GetColor(ui::kColorSysPrimary);
  }

 private:
  const bool paint_border_;
};

BEGIN_METADATA(DontSignInButton)
END_METADATA

class StartBrowsingButton : public ProfilePickerTextToolbarButton {
  METADATA_HEADER(StartBrowsingButton, ProfilePickerTextToolbarButton)

 public:
  explicit StartBrowsingButton(PressedCallback callback)
      : ProfilePickerTextToolbarButton(
            std::move(callback),
            l10n_util::GetStringUTF16(
                IDS_FRE_NATIVE_TOOLBAR_START_BROWSING_BUTTON_LABEL)) {
    SetProperty(views::kElementIdentifierKey,
                kProfilePickerToolbarStartBrowsingButtonElementId);
  }

  StartBrowsingButton(const StartBrowsingButton&) = delete;
  StartBrowsingButton& operator=(const StartBrowsingButton&) = delete;

  ~StartBrowsingButton() override = default;
};

BEGIN_METADATA(StartBrowsingButton)
END_METADATA

class EffectsControlButton : public ProfilePickerToolbarButton {
  METADATA_HEADER(EffectsControlButton, ProfilePickerToolbarButton)

 public:
  explicit EffectsControlButton(base::RepeatingCallback<void(bool)> callback)
      : ProfilePickerToolbarButton(
            base::BindRepeating(&EffectsControlButton::OnButtonPressed,
                                base::Unretained(this))),
        callback_(std::move(callback)) {
    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_PROFILE_PICKER_PAUSE_EFFECTS_BUTTON));
    SetVectorIcon(kPauseCircleIcon);
  }

  EffectsControlButton(const EffectsControlButton&) = delete;
  EffectsControlButton& operator=(const EffectsControlButton&) = delete;

  ~EffectsControlButton() override = default;

  bool effects_enabled() const { return effects_enabled_; }

 private:
  void OnButtonPressed(const ui::Event& event) {
    effects_enabled_ = !effects_enabled_;

    // Update the icon based on the new state.
    SetVectorIcon(effects_enabled_ ? kPauseCircleIcon : kPlayCircleIcon);
    // Explicitly call `UpdateIcon` as the implicit call may be omitted due to
    // the lack of a theme provider at the beginning of the first run flow.
    UpdateIcon();

    SetTooltipText(l10n_util::GetStringUTF16(
        effects_enabled_ ? IDS_PROFILE_PICKER_PAUSE_EFFECTS_BUTTON
                         : IDS_PROFILE_PICKER_PLAY_EFFECTS_BUTTON));

    callback_.Run(effects_enabled_);
  }

  bool effects_enabled_ = true;
  base::RepeatingCallback<void(bool)> callback_;
};

BEGIN_METADATA(EffectsControlButton)
END_METADATA

// A custom view targeter delegate that allows mouse events to fall through
// to the underlying WebUI/WebView in the empty space of the toolbar. This is
// specifically needed for macOS, where `views::View` hit testing intercepts
// events for the whole view bounds even without a layer.
class ProfilePickerToolbarViewTargeterDelegate
    : public views::ViewTargeterDelegate {
 public:
  ProfilePickerToolbarViewTargeterDelegate() = default;
  ProfilePickerToolbarViewTargeterDelegate(
      const ProfilePickerToolbarViewTargeterDelegate&) = delete;
  ProfilePickerToolbarViewTargeterDelegate& operator=(
      const ProfilePickerToolbarViewTargeterDelegate&) = delete;
  ~ProfilePickerToolbarViewTargeterDelegate() override = default;

  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    for (const views::View* child : target->children()) {
      if (!child->GetVisible() || !child->GetCanProcessEventsWithinSubtree()) {
        continue;
      }
      gfx::RectF converted_rect(rect);
      views::View::ConvertRectToTarget(target, child, &converted_rect);
      if (child->HitTestRect(gfx::ToEnclosingRect(converted_rect))) {
        return true;
      }
    }
    return false;
  }
};

}  // namespace

ProfilePickerToolbar::Builder::Builder(base::RepeatingClosure on_back_callback)
    : on_back_callback_(std::move(on_back_callback)) {}

ProfilePickerToolbar::Builder::~Builder() = default;

ProfilePickerToolbar::Builder::Builder(Builder&&) = default;
ProfilePickerToolbar::Builder& ProfilePickerToolbar::Builder::operator=(
    Builder&&) = default;

ProfilePickerToolbar::Builder&
ProfilePickerToolbar::Builder::WithDontSignInButton(
    base::RepeatingClosure on_dont_sign_in_callback) {
  on_dont_sign_in_callback_ = std::move(on_dont_sign_in_callback);
  return *this;
}

ProfilePickerToolbar::Builder&
ProfilePickerToolbar::Builder::WithStartBrowsingButton(
    base::RepeatingClosure on_start_browsing_callback) {
  on_start_browsing_callback_ = std::move(on_start_browsing_callback);
  return *this;
}

ProfilePickerToolbar::Builder&
ProfilePickerToolbar::Builder::WithEffectsControlButton(
    base::RepeatingCallback<void(bool)> on_effects_control_callback,
    bool visible_by_default) {
  on_effects_control_callback_ = std::move(on_effects_control_callback);
  effects_control_button_visible_by_default_ = visible_by_default;
  return *this;
}

std::unique_ptr<ProfilePickerToolbar> ProfilePickerToolbar::Builder::Build() {
  auto toolbar = base::WrapUnique(new ProfilePickerToolbar());
  toolbar->AddBackButton(on_back_callback_);
  // Add a spacer to push the subsequent button(s) to the other side.
  toolbar->AddSpacer();
  bool add_separator = false;
  if (!on_dont_sign_in_callback_.is_null()) {
    toolbar->AddDontSignInButton(
        on_dont_sign_in_callback_,
        /*paint_border=*/on_effects_control_callback_.is_null());
    add_separator = true;
  }
  if (!on_start_browsing_callback_.is_null()) {
    toolbar->AddStartBrowsingButton(on_start_browsing_callback_);
    add_separator = true;
  }
  if (!on_effects_control_callback_.is_null()) {
    if (add_separator) {
      toolbar->AddSeparator();
    }
    toolbar->AddEffectsControlButton(
        on_effects_control_callback_,
        effects_control_button_visible_by_default_);
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

  // Set the background to transparent to inherit the color from the underlying
  // WebUI / WebView.
  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));

  SetEventTargeter(std::make_unique<views::ViewTargeter>(
      std::make_unique<ProfilePickerToolbarViewTargeterDelegate>()));
}

ProfilePickerToolbar::~ProfilePickerToolbar() = default;

void ProfilePickerToolbar::AddSpacer() {
  auto spacer = std::make_unique<views::View>();
  spacer->SetCanProcessEventsWithinSubtree(false);
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
      std::make_unique<SignInBackButton>(std::move(on_back_callback)));
  sign_in_back_button_->SetVisible(false);
}

void ProfilePickerToolbar::AddDontSignInButton(
    base::RepeatingClosure on_dont_sign_in_callback,
    bool paint_border) {
  CHECK(dont_sign_in_button_ == nullptr);
  CHECK(!on_dont_sign_in_callback.is_null());
  dont_sign_in_button_ = AddChildView(std::make_unique<DontSignInButton>(
      std::move(on_dont_sign_in_callback), paint_border));
  dont_sign_in_button_->SetVisible(false);
}

void ProfilePickerToolbar::AddStartBrowsingButton(
    base::RepeatingClosure on_start_browsing_callback) {
  CHECK(start_browsing_button_ == nullptr);
  CHECK(!on_start_browsing_callback.is_null());
  start_browsing_button_ = AddChildView(std::make_unique<StartBrowsingButton>(
      std::move(on_start_browsing_callback)));
  start_browsing_button_->SetVisible(false);
}

void ProfilePickerToolbar::AddSeparator() {
  CHECK(separator_ == nullptr);
  auto separator_view = std::make_unique<views::Separator>();
  separator_view->SetOrientation(views::Separator::Orientation::kVertical);
  separator_view->SetColorId(ui::kColorSysDivider);
  separator_view->SetPreferredSize(gfx::Size(2, 16));
  // Set asymmetric horizontal margins to make the separator visually centered.
  separator_view->SetProperty(views::kMarginsKey,
                              gfx::Insets().set_left(2).set_right(6));

  // Ensure the separator's layer is non-opaque so it shows through the
  // transparent background.
  separator_view->SetPaintToLayer();
  CHECK_DEREF(separator_view->layer()).SetFillsBoundsOpaquely(false);

  separator_ = AddChildView(std::move(separator_view));
  separator_->SetVisible(false);
}

void ProfilePickerToolbar::AddEffectsControlButton(
    base::RepeatingCallback<void(bool)> on_effects_control_callback,
    bool visible_by_default) {
  CHECK(effects_control_button_ == nullptr);
  effects_control_button_ = AddChildView(std::make_unique<EffectsControlButton>(
      std::move(on_effects_control_callback)));
  effects_control_button_->SetProperty(
      views::kElementIdentifierKey,
      kProfilePickerToolbarEffectsControlButtonElementId);
  SetEffectsControlButtonVisible(visible_by_default);
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
  MaybeUpdateSeparatorVisibility();
}

void ProfilePickerToolbar::SetStartBrowsingButtonVisible(bool visible) {
  if (start_browsing_button_) {
    start_browsing_button_->SetVisible(visible);
  }
  MaybeUpdateSeparatorVisibility();
}

void ProfilePickerToolbar::SetEffectsControlButtonVisible(bool visible) {
  if (effects_control_button_) {
    effects_control_button_->SetVisible(visible);
  }
  MaybeUpdateSeparatorVisibility();
}

void ProfilePickerToolbar::MaybeUpdateSeparatorVisibility() {
  if (!separator_) {
    return;
  }
  const bool has_visible_button_before =
      (dont_sign_in_button_ && dont_sign_in_button_->GetVisible()) ||
      (start_browsing_button_ && start_browsing_button_->GetVisible());
  const bool has_visible_button_after =
      effects_control_button_ && effects_control_button_->GetVisible();

  separator_->SetVisible(has_visible_button_before && has_visible_button_after);
}

bool ProfilePickerToolbar::AreEffectsEnabled() const {
  if (!effects_control_button_) {
    return true;
  }
  return static_cast<EffectsControlButton*>(effects_control_button_)
      ->effects_enabled();
}

void ProfilePickerToolbar::SetBackButtonVisible(bool visible) {
  // The back button should always be created.
  CHECK(sign_in_back_button_);
  sign_in_back_button_->SetVisible(visible);
}

BEGIN_METADATA(ProfilePickerToolbar)
END_METADATA
