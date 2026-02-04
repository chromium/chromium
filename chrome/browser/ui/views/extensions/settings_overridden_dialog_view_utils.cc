// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/settings_overridden_dialog_view_utils.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/extensions/rich_radio_button.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace extensions {

namespace {

// This class helps make focus work correctly on the explicit-choice setting
// selection dialog. Requirements for that dialog are that (1) despite using
// radio buttons, no radio button is initially selected or focused (for the
// sake of an unbiased choice) and (2) the Save button that locks in the
// selection is disabled until a choice is made. Hence, when initially shown,
// no element on the dialog can have focus. To make this possible,
// InitialFocusTrapView is a small, user-invisible element at the top of the
// dialog, which grabs focus initially. When it loses focus (usually to a
// selected radio button), it makes itself unfocusable, so that it cannot
// regain focus later.
class InitialFocusTrapView : public views::View {
  METADATA_HEADER(InitialFocusTrapView, views::View)

 public:
  InitialFocusTrapView() {
    SetPreferredSize(gfx::Size(1, 1));
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
    GetViewAccessibility().SetName(u"Focus Trap",
                                   ax::mojom::NameFrom::kAttribute);
  }
  ~InitialFocusTrapView() override = default;

 protected:
  // views::View:
  void OnBlur() override {
    views::View::OnBlur();
    SetFocusBehavior(FocusBehavior::NEVER);
  }
};

}  // namespace

BEGIN_METADATA(InitialFocusTrapView)
END_METADATA

void AddExplicitChoiceRadioButtons(
    ui::DialogModel::Builder& builder,
    const SettingsOverriddenDialogController::SettingOption& option1,
    ui::ElementIdentifier id1,
    base::RepeatingClosure callback1,
    const SettingsOverriddenDialogController::SettingOption& option2,
    ui::ElementIdentifier id2,
    base::RepeatingClosure callback2) {
  constexpr int kExplicitChoiceRadioGroupId = 1;

  auto container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .AddChildren(
              views::Builder<views::View>(
                  std::make_unique<InitialFocusTrapView>()),
              views::Builder<views::Separator>(),
              views::Builder<RichRadioButton>(
                  std::make_unique<RichRadioButton>(
                      option1.image, option1.text, option1.description,
                      kExplicitChoiceRadioGroupId, std::move(callback1)))
                  .SetProperty(views::kElementIdentifierKey, id1),
              views::Builder<views::Separator>(),
              views::Builder<RichRadioButton>(
                  std::make_unique<RichRadioButton>(
                      option2.image, option2.text, option2.description,
                      kExplicitChoiceRadioGroupId, std::move(callback2)))
                  .SetProperty(views::kElementIdentifierKey, id2),
              views::Builder<views::Separator>())
          .Build();

  // This groups the RadioButtons contained within the RichRadioButtons (needed
  // since the RadioButtons don't share the same parent View).
  views::SetCascadingRadioGroupView(container.get(),
                                    views::kCascadingRadioGroupView);

  builder.AddCustomField(
      std::make_unique<views::BubbleDialogModelHost::CustomView>(
          std::move(container),
          views::BubbleDialogModelHost::FieldType::kControl));
}

}  // namespace extensions
