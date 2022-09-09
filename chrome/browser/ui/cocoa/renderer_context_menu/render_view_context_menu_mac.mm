// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac.h"

#include <utility>

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"

// Obj-C bridge class that is the target of all items in the context menu.
// Relies on the tag being set to the command id.

RenderViewContextMenuMac::RenderViewContextMenuMac(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params)
    : RenderViewContextMenu(render_frame_host, params),
      text_services_context_menu_(this) {}

RenderViewContextMenuMac::~RenderViewContextMenuMac() {
}

void RenderViewContextMenuMac::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == IDC_CONTENT_CONTEXT_LOOK_UP)
    LookUpInDictionary();
  else
    RenderViewContextMenu::ExecuteCommand(command_id, event_flags);
}

bool RenderViewContextMenuMac::IsCommandIdChecked(int command_id) const {
  if (text_services_context_menu_.SupportsCommand(command_id))
    return text_services_context_menu_.IsCommandIdChecked(command_id);

  if (command_id == IDC_CONTENT_CONTEXT_LOOK_UP)
    return false;

  return RenderViewContextMenu::IsCommandIdChecked(command_id);
}

bool RenderViewContextMenuMac::IsCommandIdEnabled(int command_id) const {
  if (text_services_context_menu_.SupportsCommand(command_id))
    return text_services_context_menu_.IsCommandIdEnabled(command_id);

  if (command_id == IDC_CONTENT_CONTEXT_LOOK_UP)
    return true;

  return RenderViewContextMenu::IsCommandIdEnabled(command_id);
}

std::u16string RenderViewContextMenuMac::GetSelectedText() const {
  return params_.selection_text;
}

bool RenderViewContextMenuMac::IsTextDirectionEnabled(
    base::i18n::TextDirection direction) const {
  return ParamsForTextDirection(direction) &
         blink::ContextMenuData::kCheckableMenuItemEnabled;
}

bool RenderViewContextMenuMac::IsTextDirectionChecked(
    base::i18n::TextDirection direction) const {
  return ParamsForTextDirection(direction) &
         blink::ContextMenuData::kCheckableMenuItemChecked;
}

void RenderViewContextMenuMac::UpdateTextDirection(
    base::i18n::TextDirection direction) {
  DCHECK_NE(direction, base::i18n::UNKNOWN_DIRECTION);

  int command_id = IDC_WRITING_DIRECTION_LTR;
  if (direction == base::i18n::RIGHT_TO_LEFT)
    command_id = IDC_WRITING_DIRECTION_RTL;

  content::RenderViewHost* view_host = GetRenderViewHost();
  view_host->GetWidget()->UpdateTextDirection(direction);
  view_host->GetWidget()->NotifyTextDirection();

  RenderViewContextMenu::RecordUsedItem(command_id);
}

void RenderViewContextMenuMac::AppendPlatformEditableItems() {
  text_services_context_menu_.AppendEditableItems(&menu_model_);
}

void RenderViewContextMenuMac::InitToolkitMenu() {
  if (params_.input_field_type ==
      blink::mojom::ContextMenuDataInputFieldType::kPassword)
    return;

  if (!params_.selection_text.empty() && params_.link_url.is_empty()) {
    // In case the user has selected a word that triggers spelling suggestions,
    // show the dictionary lookup under the group that contains the command to
    // “Add to Dictionary.”
    const absl::optional<size_t> index_opt =
        menu_model_.GetIndexOfCommandId(IDC_SPELLCHECK_ADD_TO_DICTIONARY);
    size_t index = index_opt.value_or(0);
    if (index_opt.has_value()) {
      while (menu_model_.GetTypeAt(index) != ui::MenuModel::TYPE_SEPARATOR) {
        index++;
      }
      ++index;  // Place it below the separator.
    }

    std::u16string printable_selection_text = PrintableSelectionText();
    EscapeAmpersands(&printable_selection_text);
    menu_model_.InsertItemAt(
        index++, IDC_CONTENT_CONTEXT_LOOK_UP,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_LOOK_UP,
                                   printable_selection_text));
    menu_model_.InsertSeparatorAt(index++, ui::NORMAL_SEPARATOR);
  }

  if (!params_.selection_text.empty())
    text_services_context_menu_.AppendToContextMenu(&menu_model_);
}

void RenderViewContextMenuMac::LookUpInDictionary() {
  content::RenderWidgetHostView* view =
      GetRenderViewHost()->GetWidget()->GetView();
  if (view)
    view->ShowDefinitionForSelection();
}

int RenderViewContextMenuMac::ParamsForTextDirection(
    base::i18n::TextDirection direction) const {
  switch (direction) {
    case base::i18n::UNKNOWN_DIRECTION:
      return params_.writing_direction_default;
    case base::i18n::RIGHT_TO_LEFT:
      return params_.writing_direction_right_to_left;
    case base::i18n::LEFT_TO_RIGHT:
      return params_.writing_direction_left_to_right;
  }
}
