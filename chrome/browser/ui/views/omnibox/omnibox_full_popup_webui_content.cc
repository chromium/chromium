// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"

#include "base/functional/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/strings/grit/ui_strings.h"

OmniboxFullPopupWebUIContent::OmniboxFullPopupWebUIContent(
    OmniboxPopupPresenterBase* presenter,
    LocationBar* location_bar,
    OmniboxController* controller)
    : OmniboxPopupWebUIContent(presenter,
                               location_bar,
                               controller,
                               /*include_location_bar_cutout=*/false,
                               /*wants_focus=*/true) {
  SetContentURL(chrome::kChromeUIOmniboxPopupURL);
}

OmniboxFullPopupWebUIContent::~OmniboxFullPopupWebUIContent() = default;

bool OmniboxFullPopupWebUIContent::EscClosesUI() const {
  return false;
}

void OmniboxFullPopupWebUIContent::CloseUI() {
  controller()->client()->FocusWebContents();
  controller()->edit_model()->OnKillFocus();

  OmniboxPopupWebUIBaseContent::CloseUI();
}

// TODO(b/504668887): If necessary, copy `OmniboxAimPopupWebUIContent::Clear()`
// implementation here to deal with tab state restoration issue(s) when the user
// creates a new tab while there's an in-progress text input in the popup.

std::string_view OmniboxFullPopupWebUIContent::GetMetricPrefix() const {
  return "Omnibox.Popup.FullWebUI";
}

// TODO(b/504669142): If necessary, copy
// `OmniboxAimPopupWebUIContent::UpdateLocationBarFocusForScreenReader()`
// implementation to here to deal with potential popup focus issue(s) when a
// screenreader is being used.

// Override of WebUIContentsWrapper::Host::HandleContextMenu. This mirrors
// content::WebContentsDelegate::HandleContextMenu, which is called by the
// WebContentsImpl to allow the delegate to handle the context menu if desired.
// Returning true means the context menu request was handled (and thus
// the caller suppresses their own context menu). Returning false allows
// the default context menu to be shown.
bool OmniboxFullPopupWebUIContent::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  if (!params.is_editable) {
    return true;
  }

  // Fetch clipboard text asynchronously and store it before showing the menu.
  GetClipboardText(
      /*notify_if_restricted=*/false,
      base::BindOnce(&OmniboxFullPopupWebUIContent::OnClipboardTextReceived,
                     weak_ptr_factory_.GetWeakPtr(), params));
  return true;
}

void OmniboxFullPopupWebUIContent::OnClipboardTextReceived(
    const content::ContextMenuParams& params,
    std::u16string clipboard_text) {
  clipboard_text_ = std::move(clipboard_text);

  params_ = params;

  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItemWithStringId(IDC_CONTENT_CONTEXT_UNDO, IDS_APP_UNDO);

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);

  gfx::Point screen_point(params.x, params.y);
  views::View::ConvertPointToScreen(this, &screen_point);

  menu_runner_->RunMenuAt(GetWidget(), /*button_controller=*/nullptr,
                          gfx::Rect(screen_point, gfx::Size()),
                          views::MenuAnchorPosition::kTopLeft,
                          ui::mojom::MenuSourceType::kMouse);
}

bool OmniboxFullPopupWebUIContent::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & blink::ContextMenuDataEditFlags::kCanUndo);
    default:
      return false;
  }
}

void OmniboxFullPopupWebUIContent::ExecuteCommand(int command_id,
                                                  int event_flags) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }
  switch (command_id) {
    case IDC_CONTENT_CONTEXT_UNDO:
      web_contents->Undo();
      break;
  }
}

BEGIN_METADATA(OmniboxFullPopupWebUIContent)
END_METADATA
