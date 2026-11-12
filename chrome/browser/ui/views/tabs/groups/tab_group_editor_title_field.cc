// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_title_field.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/ash_element_identifiers.h"
#include "base/functional/bind.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/strings/grit/ui_strings.h"
#endif

namespace {

gfx::Insets GetTitleFieldMargins() {
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  const int horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  return ui::TouchUiController::Get()->touch_ui()
             ? gfx::Insets::VH(5 * vertical_spacing / 4, horizontal_spacing)
             : gfx::Insets::VH(vertical_spacing, horizontal_spacing);
}

}  // namespace

// static
std::unique_ptr<TabGroupEditorTitleField> TabGroupEditorTitleField::Create(
    views::TextfieldController* controller,
    const std::u16string& title,
    bool stop_context_menu_propagation) {
  auto title_field =
      std::make_unique<TabGroupEditorTitleField>(stop_context_menu_propagation);
  title_field->SetText(title);
  title_field->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_TAB_GROUP_HEADER_CXMENU_TAB_GROUP_TITLE_ACCESSIBLE_NAME));
  title_field->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_BUBBLE_TITLE_PLACEHOLDER));
  title_field->set_controller(controller);
  title_field->SetProperty(views::kElementIdentifierKey,
                           kTabGroupEditorBubbleId);
  title_field->SetProperty(views::kMarginsKey, GetTitleFieldMargins());
  return title_field;
}

TabGroupEditorTitleField::TabGroupEditorTitleField(
    bool stop_context_menu_propagation)
    : stop_context_menu_propagation_(stop_context_menu_propagation) {}

TabGroupEditorTitleField::~TabGroupEditorTitleField() = default;

void TabGroupEditorTitleField::ShowContextMenu(
    const gfx::Point& p,
    ui::mojom::MenuSourceType source_type) {
  // There is no easy way to stop the propagation of a ShowContextMenu event,
  // which is sometimes used to open the bubble itself. So when the bubble is
  // opened this way, we manually hide the textfield's context menu the first
  // time. Otherwise, the textfield, which is automatically focused, would show
  // an extra context menu when the bubble first opens.
  if (stop_context_menu_propagation_) {
    stop_context_menu_propagation_ = false;
    return;
  }
  views::Textfield::ShowContextMenu(p, source_type);
}

#if BUILDFLAG(IS_CHROMEOS)
void TabGroupEditorTitleField::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == IDS_CONTENT_CONTEXT_EMOJI) {
    PreventCloseOnDeactivateForEmoji();
  }
  views::Textfield::ExecuteCommand(command_id, event_flags);
}

void TabGroupEditorTitleField::PreventCloseOnDeactivateForEmoji() {
  views::Widget* const widget = GetWidget();
  views::BubbleDialogDelegate* const bubble =
      widget && widget->widget_delegate()
          ? widget->widget_delegate()->AsBubbleDialogDelegate()
          : nullptr;
  if (!bubble) {
    return;
  }

  if (!close_on_deactivate_pin_) {
    close_on_deactivate_pin_ = bubble->PreventCloseOnDeactivate();
  }

  if (!emoji_picker_hidden_subscription_) {
    emoji_picker_hidden_subscription_ =
        ui::ElementTracker::GetElementTracker()
            ->AddElementHiddenInAnyContextCallback(
                ash::kEmojiPickerElementId,
                base::BindRepeating(
                    &TabGroupEditorTitleField::OnEmojiPickerClosed,
                    base::Unretained(this)));
  }
}

void TabGroupEditorTitleField::OnEmojiPickerClosed(
    ui::TrackedElement* element) {
  emoji_picker_hidden_subscription_ = {};
  close_on_deactivate_pin_.reset();

  // Close the bubble if it is no longer active, i.e. when the user clicks
  // elsewhere, off of both the bubble and emoji picker.
  views::Widget* const widget = GetWidget();
  if (widget && !widget->IsActive()) {
    widget->Close();
  }
}
#endif

BEGIN_METADATA(TabGroupEditorTitleField)
END_METADATA
