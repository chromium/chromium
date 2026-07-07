// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"

#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"

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
                     weak_ptr_factory_.GetWeakPtr(),
                     render_frame_host.GetGlobalId(), params));
  return true;
}

void OmniboxFullPopupWebUIContent::OnClipboardTextReceived(
    content::GlobalRenderFrameHostId render_frame_host_id,
    const content::ContextMenuParams& params,
    std::u16string clipboard_text) {
  clipboard_text_ = std::move(clipboard_text);

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id);
  if (!render_frame_host) {
    return;
  }

  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(web_contents);
  if (!menu_delegate) {
    return;
  }

  // Build the default native context menu asynchronously.
  menu_delegate->BuildMenuAsync(
      *render_frame_host, params,
      base::BindOnce(&OmniboxFullPopupWebUIContent::OnBuildMenuComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OmniboxFullPopupWebUIContent::OnBuildMenuComplete(
    std::unique_ptr<RenderViewContextMenuBase> menu) {
  if (!menu) {
    return;
  }

  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(web_contents);
  if (!menu_delegate) {
    return;
  }

  // Show the built-in native context menu as-is.
  menu_delegate->ShowMenu(std::move(menu));
}

BEGIN_METADATA(OmniboxFullPopupWebUIContent)
END_METADATA
