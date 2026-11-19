// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"

#include "base/check.h"
#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"

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
  zoom_lock_ = zoom_controller->CreateZoomDisableLock();
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::Get(web_contents->GetSiteInstance());
  zoom_map->SetTemporaryZoomLevel(
      web_contents->GetPrimaryMainFrame()->GetGlobalId(), 0.0);
}

TopChromeWebUIController::~TopChromeWebUIController() = default;

void TopChromeWebUIController::WebUIRenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  MojoWebUIController::WebUIRenderFrameCreated(render_frame_host);
  if (profile_load_time_data_json_) {
    render_frame_host->SetWebUIProperty("profileLoadTimeData",
                                        *profile_load_time_data_json_);
  }
}

// static
bool TopChromeWebUIController::IsWebUIDataSourceShared(content::WebUI* web_ui) {
  return Profile::FromWebUI(web_ui)->IsTopChromeWebUIProfile();
}

void TopChromeWebUIController::AddProfileLoadTimeData(
    content::WebUIDataSource* source,
    const base::DictValue& values) {
  if (!IsWebUIDataSourceShared(web_ui())) {
    source->AddLocalizedStrings(values);
    return;
  }
  CHECK(web_ui()->GetBindings().Has(content::BindingsPolicyValue::kWebUi));
  // Unlike the data source, the values can't be merged, so add them once.
  CHECK(!profile_load_time_data_json_);
  profile_load_time_data_json_ = base::WriteJson(values);
  CHECK(profile_load_time_data_json_);
}

content::WebUIController::DisplayDisposition
TopChromeWebUIController::GetDisplayDisposition() const {
  return content::WebUIController::DisplayDisposition::kUIElement;
}
