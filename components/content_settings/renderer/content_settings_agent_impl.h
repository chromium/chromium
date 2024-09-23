// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_RENDERER_CONTENT_SETTINGS_AGENT_IMPL_H_
#define COMPONENTS_CONTENT_SETTINGS_RENDERER_CONTENT_SETTINGS_AGENT_IMPL_H_

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
class WebFrame;
class WebURL;
}  // namespace blink

namespace content_settings {

// This class serves as an agent of the browser-side content settings machinery
// to implement browser-specified rules directly within the renderer process.
// In some cases it forwards requests on to the browser to determine policy.
// An instance of this class is associated w/ each RenderFrame in the process.
class ContentSettingsAgentImpl
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ContentSettingsAgentImpl>,
      public blink::WebContentSettingsClient,
      public mojom::ContentSettingsAgent {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Return true if this frame should be allowlisted for accessing storage.
    virtual bool IsFrameAllowlistedForStorageAccess(
        blink::WebFrame* frame) const;

    // Return true if this scheme should be allowlisted for content settings.
    virtual bool IsSchemeAllowlisted(const std::string& scheme);

    // Allows the delegate to override logic for various
    // blink::WebContentSettingsClient methods.
    virtual bool AllowReadFromClipboard();
    virtual bool AllowWriteToClipboard();
    // If an optional value is
    // returned, return std::nullopt to use the default logic.
    virtual std::optional<bool> AllowMutationEvents();
  };

  ContentSettingsAgentImpl(content::RenderFrame* render_frame,
                           std::unique_ptr<Delegate> delegate);

  ContentSettingsAgentImpl(const ContentSettingsAgentImpl&) = delete;
  ContentSettingsAgentImpl& operator=(const ContentSettingsAgentImpl&) = delete;

  ~ContentSettingsAgentImpl() override;

  // Sends an IPC notification that the specified content type was blocked.
  void DidBlockContentType(ContentSettingsType settings_type);

  // Helper to convert StorageType to its Mojo counterpart.
  static mojom::ContentSettingsManager::StorageType ConvertToMojoStorageType(
      StorageType storage_type);

  // blink::WebContentSettingsClient:
  void AllowStorageAccess(StorageType storage_type,
                          base::OnceCallback<void(bool)> callback) override;
  bool AllowStorageAccessSync(StorageType type) override;
  bool AllowReadFromClipboard() override;
  bool AllowWriteToClipboard() override;
  bool AllowMutationEvents(bool default_value) override;
  void DidNotAllowImage() override;
  void DidNotAllowScript() override;
  bool AllowRunningInsecureContent(bool allowed_per_settings,
                                   const blink::WebURL& url) override;
  bool ShouldAutoupgradeMixedContent() override;

  bool allow_running_insecure_content() const {
    return allow_running_insecure_content_;
  }

  void SetContentSettingsManager(
      mojo::Remote<mojom::ContentSettingsManager> manager) {
    content_settings_manager_ = std::move(manager);
  }

  RendererContentSettingRules* GetRendererContentSettingRules();
  void SetRendererContentSettingRulesForTest(
      const RendererContentSettingRules& rules);

 protected:
  // Allow this to be overridden by tests.
  virtual void BindContentSettingsManager(
      mojo::Remote<mojom::ContentSettingsManager>* manager);

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentSettingsAgentImplBrowserTest,
                           AllowlistedSchemes);
  FRIEND_TEST_ALL_PREFIXES(ContentSettingsAgentImplBrowserTest,
                           ContentSettingsInterstitialPages);

  // RenderFrameObserver implementation.
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void OnDestruct() override;

  // mojom::ContentSettingsAgent:
  void SetAllowRunningInsecureContent() override;
  void SendRendererContentSettingRules(
      const RendererContentSettingRules& renderer_settings) override;

  void OnContentSettingsAgentRequest(
      mojo::PendingAssociatedReceiver<mojom::ContentSettingsAgent> receiver);

  // Resets the `content_blocked_` array.
  void ClearBlockedContentSettings();

  // A getter for `content_settings_manager_` that ensures it is bound.
  mojom::ContentSettingsManager& GetContentSettingsManager();

  mojo::Remote<mojom::ContentSettingsManager> content_settings_manager_;

  // Insecure content may be permitted for the duration of this render view.
  bool allow_running_insecure_content_ = false;

  std::unique_ptr<RendererContentSettingRules> content_setting_rules_ = nullptr;

  // Stores if images, scripts, and plugins have actually been blocked.
  base::flat_set<ContentSettingsType> content_blocked_;

  // Caches the result of AllowStorageAccess.
  using StoragePermissionsKey = std::pair<url::Origin, StorageType>;
  base::flat_map<StoragePermissionsKey, bool> cached_storage_permissions_;

  std::unique_ptr<Delegate> delegate_;

  mojo::AssociatedReceiverSet<mojom::ContentSettingsAgent> receivers_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_RENDERER_CONTENT_SETTINGS_AGENT_IMPL_H_
