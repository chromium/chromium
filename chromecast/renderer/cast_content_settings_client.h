// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_CONTENT_SETTINGS_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_CONTENT_SETTINGS_CLIENT_H_

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"

namespace chromecast {

// Chromecast implementation of blink::WebContentSettingsClient.
class CastContentSettingsClient : public content::RenderFrameObserver,
                                  public blink::WebContentSettingsClient {
 public:
  CastContentSettingsClient(content::RenderFrame* render_view,
                            const std::string& app_id);
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

  std::string app_id_;
  // TODO(b/150022618): Add decisions from Cast service to control the
  // availibilitiy of the Renderer features.
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_CONTENT_SETTINGS_CLIENT_H_
