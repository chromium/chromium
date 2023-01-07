// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_CONTENT_SETTINGS_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_CONTENT_SETTINGS_CLIENT_H_

#include "chromecast/common/mojom/metrics_helper.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"

namespace chromecast {

// Chromecast implementation of blink::WebContentSettingsClient.
class CastContentSettingsClient : public content::RenderFrameObserver,
                                  public blink::WebContentSettingsClient {
 public:
  CastContentSettingsClient(content::RenderFrame* render_view,
                            const std::string& app_id,
                            bool allow_insecure_content);
  CastContentSettingsClient(const CastContentSettingsClient&) = delete;
  CastContentSettingsClient& operator=(const CastContentSettingsClient&) =
      delete;

 private:
  ~CastContentSettingsClient() override;

  // content::RenderFrameObserver implementation.
  void OnDestruct() override;

  // blink::WebContentSettingsClient implementation.
  bool AllowRunningInsecureContent(bool enabled_per_settings,
                                   const blink::WebURL& url) override;
  void PassiveInsecureContentFound(const blink::WebURL&) override;
  bool ShouldAutoupgradeMixedContent() override;

  void ReportRendererFeatureUse(const std::string& app_id,
                                const std::string& feature_name);

  const std::string app_id_;
  const bool allow_insecure_content_;
  // TODO(b/150022618): Add decisions from Cast service to control the
  // availibilitiy of the Renderer features.

  mojo::Remote<metrics::mojom::MetricsHelper> metrics_helper_remote_;
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_CONTENT_SETTINGS_CLIENT_H_
