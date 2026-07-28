// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/touch_selection/touch_editing_controller.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/widget.h"

OmniboxFullPopupWebUIContent::OmniboxFullPopupWebUIContent(
    OmniboxPopupPresenterBase* presenter,
    LocationBar* location_bar,
    OmniboxController* controller)
    : OmniboxPopupWebUIContent(presenter,
                               location_bar,
                               controller,
                               /*include_location_bar_cutout=*/false,
                               /*wants_focus=*/true),
      OmniboxContextMenuMixin<ui::SimpleMenuModel::Delegate>(location_bar,
                                                             controller) {
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
  PrepareToShowContextMenu(
      base::BindOnce(&OmniboxFullPopupWebUIContent::ShowContextMenuComplete,
                     weak_ptr_factory_.GetWeakPtr(), params));
  return true;
}

void OmniboxFullPopupWebUIContent::ShowContextMenuComplete(
    const content::ContextMenuParams& params) {
  params_ = params;

  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

  content::WebContents* web_contents = GetWebContents();
  AddTextfieldItems(web_contents ? web_contents->GetWeakPtr()
                                 : base::WeakPtr<content::WebContents>(),
                    params, menu_model_.get());
  AddOmniboxSpecificItems(menu_model_.get());

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

void OmniboxFullPopupWebUIContent::ExecuteCommand(int command_id,
                                                  int event_flags) {
  if (HandleExecuteCommand(command_id, event_flags)) {
    return;
  }
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }
  HandleExecuteTextEditingCommandOnWebContents(web_contents, command_id,
                                               event_flags);
}

bool OmniboxFullPopupWebUIContent::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return HandleGetAcceleratorForCommandId(command_id, accelerator);
}

bool OmniboxFullPopupWebUIContent::IsContextMenuForReadOnlyOmnibox() const {
  return !params_.is_editable;
}

const gfx::FontList& OmniboxFullPopupWebUIContent::FontListForContextMenu()
    const {
  return views::TypographyProvider::Get().GetFont(views::style::CONTEXT_MENU,
                                                  views::style::STYLE_PRIMARY);
}

bool OmniboxFullPopupWebUIContent::IsContextMenuTextEditingCommandEnabled(
    int command_id) const {
  return HandleIsContextMenuTextEditingCommandEnabled(command_id, params_);
}

views::Widget* OmniboxFullPopupWebUIContent::GetWidgetForTextServices() {
  return GetWidget();
}

BEGIN_METADATA(OmniboxFullPopupWebUIContent)
END_METADATA
