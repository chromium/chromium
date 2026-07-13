// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
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
#include "ui/strings/grit/ui_strings.h"
#include "ui/touch_selection/touch_editing_controller.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/widget.h"

namespace {

class FullPopupTextfield : public views::Textfield {
 public:
  explicit FullPopupTextfield(OmniboxFullPopupWebUIContent* owner)
      : owner_(owner) {}

  const views::Widget* GetWidget() const override {
    return owner_->GetWidget();
  }
  views::Widget* GetWidget() override { return owner_->GetWidget(); }

#if BUILDFLAG(IS_MAC)
  bool SupportsLookUp() const override {
    return !features::IsMenuSimplificationEnabled();
  }
#endif

  bool SupportsEmoji() const override {
    return !features::IsMenuSimplificationEnabled();
  }

#if BUILDFLAG(IS_MAC)
  bool SupportsEditableContextMenuItems() const override { return false; }
#endif

  bool GetWordLookupDataFromSelection(gfx::DecoratedText* decorated_text,
                                      gfx::Rect* rect) override {
#if BUILDFLAG(IS_MAC)
    if (content::WebContents* web_contents = owner_->GetWebContents()) {
      if (content::RenderWidgetHostView* view =
              web_contents->GetRenderWidgetHostView()) {
        view->ShowDefinitionForSelection();
      }
    }
#endif
    return false;
  }

 private:
  raw_ptr<OmniboxFullPopupWebUIContent> owner_;
};

}  // namespace

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

  context_menu_textfield_helper_ = std::make_unique<FullPopupTextfield>(this);
  context_menu_textfield_helper_->SetReadOnly(!params_.is_editable);
  context_menu_textfield_helper_->SetText(params_.selection_text);
  context_menu_textfield_helper_->SelectAll(false);

  context_menu_textfield_helper_->UpdateContextMenuContents(menu_model_.get());

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
  switch (command_id) {
    case views::Textfield::kUndo:
      web_contents->Undo();
      return;
    case std::to_underlying(ui::TouchEditable::MenuCommands::kCut):
      web_contents->Cut();
      return;
    case std::to_underlying(ui::TouchEditable::MenuCommands::kCopy):
      web_contents->Copy();
      return;
    case std::to_underlying(ui::TouchEditable::MenuCommands::kPaste):
      web_contents->Paste();
      return;
    case views::Textfield::kDelete:
      web_contents->Delete();
      return;
    case std::to_underlying(ui::TouchEditable::MenuCommands::kSelectAll):
      web_contents->SelectAll();
      return;
  }
  context_menu_textfield_helper_->ExecuteCommand(command_id, event_flags);
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
  switch (command_id) {
    case views::Textfield::kUndo:
      return !!(params_.edit_flags & blink::ContextMenuDataEditFlags::kCanUndo);
    case std::to_underlying(ui::TouchEditable::MenuCommands::kCut):
      return !!(params_.edit_flags & blink::ContextMenuDataEditFlags::kCanCut);
    case std::to_underlying(ui::TouchEditable::MenuCommands::kCopy):
      return !!(params_.edit_flags & blink::ContextMenuDataEditFlags::kCanCopy);
    case std::to_underlying(ui::TouchEditable::MenuCommands::kPaste):
      return !!(params_.edit_flags &
                blink::ContextMenuDataEditFlags::kCanPaste);
    case views::Textfield::kDelete:
      return !!(params_.edit_flags &
                blink::ContextMenuDataEditFlags::kCanDelete);
    case std::to_underlying(ui::TouchEditable::MenuCommands::kSelectAll):
      return !!(params_.edit_flags &
                blink::ContextMenuDataEditFlags::kCanSelectAll);
    default:
      return context_menu_textfield_helper_->IsCommandIdEnabled(command_id);
  }
}

BEGIN_METADATA(OmniboxFullPopupWebUIContent)
END_METADATA
