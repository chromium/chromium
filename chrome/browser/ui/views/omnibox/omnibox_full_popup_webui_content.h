// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_FULL_POPUP_WEBUI_CONTENT_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_FULL_POPUP_WEBUI_CONTENT_H_

#include <memory>
#include <optional>
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

  void Clear() override;

  // Requests the WebUI searchbox context menu at `screen_point`, shown once
  // the popup widget is active and the `<input>` is focused.
  bool RequestContextMenu(const gfx::Point& screen_point,
                          ui::mojom::MenuSourceType source_type);

  void OnPopupWidgetActivationChanged(bool active);

  // Whether the popup has presented a frame since it was last shown.
  void OnPopupWidgetPresentedChanged(bool presented);

  void CancelPendingContextMenuRequest();

  // content::WebContentsObserver:
  void OnFocusChangedInPage(
      const content::FocusedNodeDetails& details) override;

 private:
  friend class OmniboxFullPopupWebUIContentBrowserTest;

  void OnClearCallback();
  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // OmniboxContextMenuMixinBase:
  bool IsContextMenuForReadOnlyOmnibox() const override;
  const gfx::FontList& FontListForContextMenu() const override;
  bool IsContextMenuTextEditingCommandEnabled(int command_id) const override;
  views::Widget* GetWidgetForTextServices() override;

 protected:
  std::string_view GetMetricPrefix() const override;

 private:
  // WebUIContentsWrapper::Host:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  void ShowContextMenuComplete(const content::ContextMenuParams& params);
  void ShowPendingContextMenuIfReady();

  OmniboxPopupHandler* GetPopupHandler() {
    return const_cast<OmniboxPopupHandler*>(
        std::as_const(*this).GetPopupHandler());
  }
  const OmniboxPopupHandler* GetPopupHandler() const;

  content::ContextMenuParams params_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  struct ContextMenuRequest {
    // In screen coordinates; converted when the menu is shown, since the popup
    // may not be positioned yet when the request is made.
    gfx::Point screen_point;
    ui::mojom::MenuSourceType source_type;
  };
  std::optional<ContextMenuRequest> pending_context_menu_request_;
  bool popup_presented_ = false;

  base::WeakPtrFactory<OmniboxFullPopupWebUIContent> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */,
                   OmniboxFullPopupWebUIContent,
                   OmniboxPopupWebUIContent)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, OmniboxFullPopupWebUIContent)

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_FULL_POPUP_WEBUI_CONTENT_H_
