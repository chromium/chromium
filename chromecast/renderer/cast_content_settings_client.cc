// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_settings_client.h"

#include "base/strings/strcat.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {

CastContentSettingsClient::CastContentSettingsClient(
    content::RenderFrame* render_frame,
    const std::string& app_id,
    bool allow_insecure_content)
    : content::RenderFrameObserver(render_frame),
      app_id_(app_id),
      allow_insecure_content_(allow_insecure_content) {
  render_frame->GetWebFrame()->SetContentSettingsClient(this);
  content::RenderThread::Get()->BindHostReceiver(
      metrics_helper_remote_.BindNewPipeAndPassReceiver());
}

CastContentSettingsClient::~CastContentSettingsClient() {}

void CastContentSettingsClient::OnDestruct() {
  delete this;
}

bool CastContentSettingsClient::AllowRunningInsecureContent(
    bool enabled_per_settings,
    const blink::WebURL& url) {
  ReportRendererFeatureUse(app_id_, "ActiveInsecureContent");
  return allow_insecure_content_;
}

void CastContentSettingsClient::PassiveInsecureContentFound(
    const blink::WebURL&) {
  ReportRendererFeatureUse(app_id_, "PassiveInsecureContent");
}

bool CastContentSettingsClient::ShouldAutoupgradeMixedContent() {
  ReportRendererFeatureUse(app_id_, "DisableAutoUpgradeMixedContent");
  return !allow_insecure_content_;
}

void CastContentSettingsClient::ReportRendererFeatureUse(
    const std::string& app_id,
    const std::string& feature_name) {
  std::string event =
      base::StrCat({"Cast.Platform.RendererFeatureUse.", feature_name});
  metrics_helper_remote_->RecordApplicationEvent(app_id,
                                                 /*session_id=*/"",
                                                 /*sdk_version=*/"", event);
}

}  // namespace chromecast
