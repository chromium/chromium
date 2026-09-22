// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_drag_state.h"

namespace webui_toolbar {

// static
WebUIToolbarDragState* WebUIToolbarDragState::GetOrCreateForWebContents(
    content::WebContents* contents) {
  CreateForWebContents(contents);
  return FromWebContents(contents);
}

// static
bool WebUIToolbarDragState::TakeDragOriginatedFromRenderer(
    content::WebContents* contents) {
  if (!contents) {
    return false;
  }
  WebUIToolbarDragState* drag_state = FromWebContents(contents);
  if (!drag_state) {
    return false;
  }

  // Clear the drag state immediately after reading to ensure that the
  // "renderer-tainted" drag state does not persist and pollute subsequent
  // generic non-drag navigations (e.g. if the WebUI triggers
  // Navigate/NavigateText outside a drag transaction).
  bool val = drag_state->drag_originated_from_renderer();
  drag_state->set_drag_originated_from_renderer(false);
  return val;
}

// static
bool WebUIToolbarDragState::TakeDragHasJavaScriptUrl(
    content::WebContents* contents) {
  if (!contents) {
    return false;
  }
  WebUIToolbarDragState* drag_state = FromWebContents(contents);
  if (!drag_state) {
    return false;
  }

  bool val = drag_state->drag_has_javascript_url();
  drag_state->set_drag_has_javascript_url(false);
  return val;
}

WebUIToolbarDragState::WebUIToolbarDragState(content::WebContents* contents)
    : content::WebContentsUserData<WebUIToolbarDragState>(*contents) {}

WebUIToolbarDragState::~WebUIToolbarDragState() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebUIToolbarDragState);

}  // namespace webui_toolbar
