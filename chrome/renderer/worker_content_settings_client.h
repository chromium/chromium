// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_H_
#define CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_H_

#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrame;
}  // namespace content

struct RendererContentSettingRules;

// This client is created on the main renderer thread then passed onto the
// blink's worker thread. For workers created from other workers, Clone()
// is called on the "parent" worker's thread.
class WorkerContentSettingsClient : public blink::WebContentSettingsClient {
 public:
  explicit WorkerContentSettingsClient(content::RenderFrame* render_frame);

  WorkerContentSettingsClient& operator=(const WorkerContentSettingsClient&) =
      delete;

  ~WorkerContentSettingsClient() override;

  // WebContentSettingsClient overrides.
  std::unique_ptr<blink::WebContentSettingsClient> Clone() override;
  void AllowStorageAccess(StorageType storage_type,
                          base::OnceCallback<void(bool)> callback) override;
  bool AllowStorageAccessSync(StorageType storage_type) override;
  bool AllowRunningInsecureContent(bool allowed_per_settings,
                                   const blink::WebURL& url) override;
  bool ShouldAutoupgradeMixedContent() override;

 private:
  explicit WorkerContentSettingsClient(
      const WorkerContentSettingsClient& other);
  void EnsureContentSettingsManager() const;

  // Loading document context for this worker.
  bool is_unique_origin_ = false;
  url::Origin document_origin_;
  net::SiteForCookies site_for_cookies_;
  url::Origin top_frame_origin_;
  bool allow_running_insecure_content_;
  const blink::LocalFrameToken frame_token_;
  std::unique_ptr<RendererContentSettingRules> content_setting_rules_;

  // Because instances of this class are created on the parent's thread (i.e,
  // on the renderer main thread or on the thread of the parent worker), it is
  // necessary to lazily bind the `content_settings_manager_` remote. The
  // pending remote is initialized on the parent thread and then the remote is
  // bound when needed on the worker's thread.
  mutable mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
      pending_content_settings_manager_;
  mutable mojo::Remote<content_settings::mojom::ContentSettingsManager>
      content_settings_manager_;
};

#endif  // CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_H_
