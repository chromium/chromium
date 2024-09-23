// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_CONTENT_SETTINGS_MANAGER_IMPL_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_CONTENT_SETTINGS_MANAGER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

namespace content_settings {
class CookieSettings;
}  // namespace content_settings

namespace net {
class SiteForCookies;
}  // namespace net

namespace content_settings {

class ContentSettingsManagerImpl
    : public content_settings::mojom::ContentSettingsManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Gets cookie settings for this browser context.
    virtual scoped_refptr<CookieSettings> GetCookieSettings(
        content::BrowserContext* browser_context) = 0;

    // Allows delegate to override AllowStorageAccess(). If the delegate returns
    // true here, the default logic will be bypassed. Can be called on any
    // thread.
    virtual bool AllowStorageAccess(
        const content::GlobalRenderFrameHostToken& frame_token,
        StorageType storage_type,
        const GURL& url,
        bool allowed,
        base::OnceCallback<void(bool)>* callback) = 0;

    // Returns a new instance of this delegate.
    virtual std::unique_ptr<Delegate> Clone() = 0;
  };

  ~ContentSettingsManagerImpl() override;

  static void Create(
      content::RenderProcessHost* render_process_host,
      mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
          receiver,
      std::unique_ptr<Delegate> delegate);

  // mojom::ContentSettingsManager methods:
  void Clone(
      mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
          receiver) override;
  void AllowStorageAccess(const blink::LocalFrameToken& frame_token,
                          StorageType storage_type,
                          const url::Origin& origin,
                          const net::SiteForCookies& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          base::OnceCallback<void(bool)> callback) override;
  void OnContentBlocked(const blink::LocalFrameToken& frame_token,
                        ContentSettingsType type) override;

 private:
  ContentSettingsManagerImpl(int render_process_id,
                             std::unique_ptr<Delegate> delegate,
                             scoped_refptr<CookieSettings> cookie_settings);
  ContentSettingsManagerImpl(const ContentSettingsManagerImpl& other);

  static void CreateOnThread(
      int render_process_id,
      mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
          receiver,
      scoped_refptr<CookieSettings> cookie_settings,
      std::unique_ptr<ContentSettingsManagerImpl::Delegate> delegate);

  std::unique_ptr<Delegate> delegate_;

  // Use these IDs to hold a weak reference back to the RenderFrameHost.
  const int render_process_id_;

  // Used to look up storage permissions.
  const scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_CONTENT_SETTINGS_MANAGER_IMPL_H_
