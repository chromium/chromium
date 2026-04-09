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
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace extensions {

void AddDialogContent(
    ui::DialogModel::Builder& builder,
    std::u16string dialog_description,
    ui::ElementIdentifier paragraph_id,
    const SettingsOverriddenDialogController::SettingOption& option1,
    ui::ElementIdentifier id1,
    base::RepeatingClosure callback1,
    const SettingsOverriddenDialogController::SettingOption& option2,
    ui::ElementIdentifier id2,
    base::RepeatingClosure callback2) {
  constexpr int kExplicitChoiceRadioGroupId = 1;

  // This dialog can have no initially-focused button, because neither radio
  // can be selected or have a focus ring, and the Save button is disabled until
  // a choice is made. This creates an interesting situation where we need to
  // make sure focus lands somewhere safe, and screen-readers announce the
  // dialog properly. This is important because this dialog is meant to be
  // non-escapable, so screen-readers need to work well.
  //
  // On Windows, we need to focus the paragraph element (which is explicitly
  // made focusable when created), or else the dialog does not grab focus when
  // it appears (ie. nothing is read, and a Tab keystroke is required to select
  // the dialog).
  //
  // On Mac, the dialog heading is picked up as the initially-focused element
  // and the dialog is properly introduced by screen readers. If we set an
  // initially-focused field, the focus appears to change within the dialog
  // immediately after it appears, which interrupts introduction of the dialog
  // by the screen reader.
#if BUILDFLAG(IS_WIN)
  const bool initially_focus_description = true;
#else
  const bool initially_focus_description = false;
#endif

  const auto* const layout_provider = views::LayoutProvider::Get();
  views::Label* paragraph_label = nullptr;
  auto container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .AddChildren(
              views::Builder<views::Label>()
                  .CopyAddressTo(&paragraph_label)
                  .SetProperty(views::kElementIdentifierKey, paragraph_id)
                  .SetText(dialog_description)
                  .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                  .SetTextStyle(views::style::STYLE_PRIMARY)
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetFocusBehavior(initially_focus_description
                                        ? views::View::FocusBehavior::ALWAYS
                                        : views::View::FocusBehavior::NEVER),
              views::Builder<views::View>().SetPreferredSize(gfx::Size(
                  1, layout_provider->GetDistanceMetric(
                         views::DISTANCE_UNRELATED_CONTROL_VERTICAL))),
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
          views::BubbleDialogModelHost::FieldType::kControl, paragraph_label),
      paragraph_id);

  if (initially_focus_description) {
    builder.SetInitiallyFocusedField(paragraph_id);
  }
}

}  // namespace extensions
