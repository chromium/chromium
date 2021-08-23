// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_SERVICE_H_
#define CHROMECAST_BROWSER_CAST_WEB_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "chromecast/common/identification_settings_manager.h"
#include "chromecast/common/mojom/identification_settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class BrowserContext;
class StoragePartition;
}  // namespace content

namespace chromecast {

class CastWebViewFactory;
class CastWindowManager;
class LRURendererCache;

// This class dispenses CastWebView objects which are used to wrap WebContents
// in cast_shell. This class temporarily takes ownership of CastWebViews when
// they go out of scope, allowing us to keep the pages alive for extra time if
// needed. CastWebService allows us to synchronously destroy all pages when the
// system is shutting down, preventing use of freed browser resources.
class CastWebService : public mojom::CastWebService,
                       public mojom::BrowserIdentificationSettingsManager {
 public:
  CastWebService(content::BrowserContext* browser_context,
                 CastWebViewFactory* web_view_factory,
                 CastWindowManager* window_manager);
  CastWebService(const CastWebService&) = delete;
  CastWebService& operator=(const CastWebService&) = delete;
  ~CastWebService() override;

  // This is a temporary method to allow in-process embedders to directly own
  // the CastWebView. This will be removed once the lifetime of CastWebview is
  // scoped by the CastWebContents mojo::Remote.
  CastWebView::Scoped CreateWebViewInternal(
      const CastWebView::CreateParams& create_params,
      mojom::CastWebViewParamsPtr params);

  // This implementation varies by platform (Aura vs Android).
  std::unique_ptr<CastContentWindow> CreateWindow(
      base::WeakPtr<CastContentWindow::Delegate> delegate,
      mojom::CastWebViewParamsPtr params);

  content::BrowserContext* browser_context() { return browser_context_; }
  LRURendererCache* overlay_renderer_cache() {
    return overlay_renderer_cache_.get();
  }

  void FlushDomLocalStorage();

  // |callback| is called when data deletion is done or at least the deletion
  // is scheduled.
  void ClearLocalStorage(base::OnceClosure callback);

  // mojom::CastWebService implementation:
  void CreateWebView(
      mojom::CastWebViewParamsPtr params,
      mojo::PendingReceiver<mojom::CastWebContents> web_contents,
      mojo::PendingReceiver<mojom::CastContentWindow> window) override;
  void RegisterWebUiClient(mojo::PendingRemote<mojom::WebUiClient> client,
                           const std::vector<std::string>& hosts) override;

  // mojom::BrowserIdentificationSettingsManager implementation:
  void CreateSessionWithSubstitutions(
      const std::string& session_id,
      std::vector<mojom::SubstitutableParameterPtr> params) override;
  void SetClientAuthForSession(const std::string& session_id,
                               mojo::PendingRemote<mojom::ClientAuthDelegate>
                                   client_auth_delegate) override;
  void UpdateAppSettingsForSession(const std::string& session_id,
                                   mojom::AppSettingsPtr app_settings) override;
  void UpdateDeviceSettingsForSession(
      const std::string& session_id,
      mojom::DeviceSettingsPtr device_settings) override;
  void UpdateSubstitutableParamValuesForSession(
      const std::string& session_id,
      std::vector<mojom::IndexValuePairPtr> updated_values) override;
  void UpdateBackgroundModeForSession(const std::string& session_id,
                                      bool background_mode) override;
  void OnSessionDestroyed(const std::string& session_id) override;

  scoped_refptr<CastURLLoaderThrottle::Delegate>
  GetURLLoaderThrottleDelegateForSession(const std::string& session_id);

  // Immediately deletes all owned CastWebViews. This should happen before
  // CastWebService is deleted, to prevent UAF of shared browser objects.
  void DeleteExpiringWebViews();

 private:
  void OwnerDestroyed(CastWebView* web_view);
  void DeleteWebView(CastWebView* web_view);

  IdentificationSettingsManager* GetSessionManager(
      const std::string& session_id);

  content::BrowserContext* const browser_context_;
  CastWebViewFactory* const web_view_factory_;
  CastWindowManager* const window_manager_;

  // If a CastWebView is marked for delayed deletion, it is temporarily held
  // here for a short time after the original owner releases the CastWebView.
  // After the desired shutdown time elapses, the CastWebView is deleted.
  base::flat_set<std::unique_ptr<CastWebView>> expiring_web_views_;

  const std::unique_ptr<LRURendererCache> overlay_renderer_cache_;
  bool immediately_delete_webviews_ = false;

  base::flat_map<std::string /* session_id */,
                 scoped_refptr<IdentificationSettingsManager>>
      settings_managers_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtr<CastWebService> weak_ptr_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastWebService> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_SERVICE_H_
