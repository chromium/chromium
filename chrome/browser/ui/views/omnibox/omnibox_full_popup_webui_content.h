// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_FULL_POPUP_WEBUI_CONTENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_FULL_POPUP_WEBUI_CONTENT_H_

#include <memory>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu_mixin.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/metadata/view_factory.h"

class LocationBar;
class OmniboxPopupPresenterBase;
class OmniboxController;

namespace views {
class Textfield;
}  // namespace views

// The content WebView for the full popup (input row + suggestions dropdown) of
// a WebUI Omnibox.
class OmniboxFullPopupWebUIContent
    : public OmniboxPopupWebUIContent,
      public OmniboxContextMenuMixin<ui::SimpleMenuModel::Delegate> {
  METADATA_HEADER(OmniboxFullPopupWebUIContent, OmniboxPopupWebUIContent)

 public:
  OmniboxFullPopupWebUIContent() = delete;
  OmniboxFullPopupWebUIContent(OmniboxPopupPresenterBase* presenter,
                               LocationBar* location_bar,
                               OmniboxController* controller);
  OmniboxFullPopupWebUIContent(const OmniboxFullPopupWebUIContent&) = delete;
  OmniboxFullPopupWebUIContent& operator=(const OmniboxFullPopupWebUIContent&) =
      delete;
  ~OmniboxFullPopupWebUIContent() override;

  bool EscClosesUI() const override;

  void CloseUI() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // OmniboxContextMenuMixinBase:
  bool IsContextMenuForReadOnlyOmnibox() const override;
  const gfx::FontList& FontListForContextMenu() const override;
  bool IsContextMenuTextEditingCommandEnabled(int command_id) const override;

 protected:
  std::string_view GetMetricPrefix() const override;

 private:
  // WebUIContentsWrapper::Host:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  void ShowContextMenuComplete(const content::ContextMenuParams& params);

  content::ContextMenuParams params_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // A helper `Textfield` instance used solely for generating and handling the
  // native context menu when the user right-clicks inside the full WebUI
  // popup's "input row" (i.e. this `Textfield` is NOT painted on-screen).
  //
  // Although the "input row" is rendered in WebUI, we intercept right-clicks to
  // show a native context menu that matches the `OmniboxViewViews` context
  // menu (`HandleContextMenu()`). Without this helper, populating standard text
  // editing items (Undo, Cut, Select All, Emoji, Look Up, etc.) across all
  // platforms would require duplicating substantial native context menu
  // construction logic.
  //
  // Thus, by maintaining this proxy `Textfield` instance, we can call
  // `UpdateContextMenuContents()` to generate the core native context menu
  // structure and delegate any auxiliary or platform-specific command
  // execution/enablement to `context_menu_textfield_helper_` when those
  // commands are not directly handled by the WebUI itself.
  std::unique_ptr<views::Textfield> context_menu_textfield_helper_;

  base::WeakPtrFactory<OmniboxFullPopupWebUIContent> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   OmniboxFullPopupWebUIContent,
                   OmniboxPopupWebUIContent)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, OmniboxFullPopupWebUIContent)

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_FULL_POPUP_WEBUI_CONTENT_H_
