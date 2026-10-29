// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/content_settings_manager_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content_settings {
namespace {

using StorageType = mojom::ContentSettingsManager::StorageType;

// The delegate for a browser context without cookie settings.
class NoCookieSettingsDelegate : public ContentSettingsManagerImpl::Delegate {
 public:
  scoped_refptr<CookieSettings> GetCookieSettings(
      content::BrowserContext* browser_context) override {
    return nullptr;
  }

  bool AllowStorageAccess(
      const content::GlobalRenderFrameHostToken& frame_token,
      StorageType storage_type,
      const GURL& url,
      bool allowed,
      base::OnceCallback<void(bool)>* callback) override {
    return false;
  }

  std::unique_ptr<ContentSettingsManagerImpl::Delegate> Clone() override {
    return std::make_unique<NoCookieSettingsDelegate>();
  }
};

using ContentSettingsManagerImplTest = content::RenderViewHostTestHarness;

// Tests that a browser context without cookie settings gets no storage access.
TEST_F(ContentSettingsManagerImplTest, NoStorageAccessWithoutCookieSettings) {
  mojo::Remote<mojom::ContentSettingsManager> manager;
  ContentSettingsManagerImpl::Create(
      process(), manager.BindNewPipeAndPassReceiver(),
      std::make_unique<NoCookieSettingsDelegate>());
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  const net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromOrigin(origin);

  base::test::TestFuture<bool> is_allowed;
  manager->IsStorageAccessAllowed(origin, site_for_cookies, origin,
                                  is_allowed.GetCallback());
  EXPECT_FALSE(is_allowed.Get());

  base::test::TestFuture<bool> allow;
  manager->AllowStorageAccess(main_rfh()->GetFrameToken(),
                              StorageType::LOCAL_STORAGE, origin,
                              site_for_cookies, origin, allow.GetCallback());
  EXPECT_FALSE(allow.Get());
}

}  // namespace
}  // namespace content_settings
