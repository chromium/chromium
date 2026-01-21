// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"

#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_ui.h"

TopChromeWebUIController::TopChromeWebUIController(
    content::WebUI* contents,
    bool enable_chrome_send,
    bool enable_chrome_histograms)
    : MojoWebUIController(contents,
                          enable_chrome_send,
                          enable_chrome_histograms) {}

void TopChromeWebUIController::WebUIPrimaryPageChanged(content::Page& page) {
  MojoWebUIController::WebUIPrimaryPageChanged(page);
  // Manually set zoom level to 100% and disable zoom mode.
  content::WebContents* web_contents = web_ui()->GetWebContents();

  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  if (!zoom_controller) {
    zoom_controller = zoom::ZoomController::CreateForWebContents(web_contents);
  }
  zoom_controller->SetZoomMode(zoom::ZoomController::ZOOM_MODE_DISABLED);
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::Get(web_contents->GetSiteInstance());
  zoom_map->SetTemporaryZoomLevel(
      web_contents->GetPrimaryMainFrame()->GetGlobalId(), 0.0);
}

TopChromeWebUIController::~TopChromeWebUIController() = default;
