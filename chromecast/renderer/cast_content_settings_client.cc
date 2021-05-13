// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_settings_client.h"

#include "base/metrics/user_metrics.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace {

void ReportRendererFeatureUse(const std::string& app_id,
                              const std::string& feature_name) {
  std::string action = "Cast.Platform.RendererFeatureUse.";
  action.append(feature_name);
  action.append(".");
  action.append(app_id);
  base::RecordComputedAction(action);
}

}  // namespace

namespace chromecast {

CastContentSettingsClient::CastContentSettingsClient(
    content::RenderFrame* render_frame,
    const std::string& app_id)
    : content::RenderFrameObserver(render_frame), app_id_(app_id) {
  render_frame->GetWebFrame()->SetContentSettingsClient(this);
}

CastContentSettingsClient::~CastContentSettingsClient() {}

bool CastContentSettingsClient::AllowRunningInsecureContent(
    bool enabled_per_settings,
    const blink::WebURL& url) {
  ReportRendererFeatureUse(app_id_, "ActiveInsecureContent");
  return true;
}

void CastContentSettingsClient::PassiveInsecureContentFound(
    const blink::WebURL&) {
  ReportRendererFeatureUse(app_id_, "PassiveInsecureContent");
}

bool CastContentSettingsClient::ShouldAutoupgradeMixedContent() {
  ReportRendererFeatureUse(app_id_, "DisableAutoUpgradeMixedContent");
  return false;
}

void CastContentSettingsClient::OnDestruct() {
  delete this;
}

}  // namespace chromecast
