// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_RENDERER_CONTENT_SETTINGS_AGENT_IMPL_H_
#define COMPONENTS_CONTENT_SETTINGS_RENDERER_CONTENT_SETTINGS_AGENT_IMPL_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
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

    // Return true if this scheme should be whitelisted for content settings.
    virtual bool IsSchemeWhitelisted(const std::string& scheme);

    // Allows the delegate to override logic for various
    // blink::WebContentSettingsClient methods. If an optional value is
    // returned, return base::nullopt to use the default logic.
    virtual base::Optional<bool> AllowReadFromClipboard();
    virtual base::Optional<bool> AllowWriteToClipboard();
    virtual base::Optional<bool> AllowMutationEvents();
    virtual void PassiveInsecureContentFound(const blink::WebURL& resource_url);
  };

  // Set |should_whitelist| to true if |render_frame()| contains content that
  // should be whitelisted for content settings.
  ContentSettingsAgentImpl(content::RenderFrame* render_frame,
                           bool should_whitelist,
                           std::unique_ptr<Delegate> delegate);
  ~ContentSettingsAgentImpl() override;

  // Sets the content setting rules which back |allowImage()|, |allowScript()|,
  // |allowScriptFromSource()|. |content_setting_rules| must outlive this
  // |ContentSettingsAgentImpl|.
  void SetContentSettingRules(
      const RendererContentSettingRules* content_setting_rules);
  const RendererContentSettingRules* GetContentSettingRules();

  // Sends an IPC notification that the specified content type was blocked.
  void DidBlockContentType(ContentSettingsType settings_type);

  // Helper to convert StorageType to its Mojo counterpart.
  static mojom::ContentSettingsManager::StorageType ConvertToMojoStorageType(
      StorageType storage_type);

  // blink::WebContentSettingsClient:
  void AllowStorageAccess(StorageType storage_type,
                          base::OnceCallback<void(bool)> callback) override;
  bool AllowStorageAccessSync(StorageType type) override;
  bool AllowImage(bool enabled_per_settings,
                  const blink::WebURL& image_url) override;
  bool AllowScript(bool enabled_per_settings) override;
  bool AllowScriptFromSource(bool enabled_per_settings,
                             const blink::WebURL& script_url) override;
  bool AllowReadFromClipboard(bool default_value) override;
  bool AllowWriteToClipboard(bool default_value) override;
  bool AllowMutationEvents(bool default_value) override;
  void DidNotAllowPlugins() override;
  void DidNotAllowScript() override;
  bool AllowRunningInsecureContent(bool allowed_per_settings,
                                   const blink::WebURL& url) override;
  bool AllowPopupsAndRedirects(bool default_value) override;
  void PassiveInsecureContentFound(const blink::WebURL& resource_url) override;
  bool ShouldAutoupgradeMixedContent() override;

  bool allow_running_insecure_content() const {
    return allow_running_insecure_content_;
  }

  // Allow passing both WebURL and GURL here, so that we can early return
  // without allocating a new backing string if only the default rule matches.
  ContentSetting GetContentSettingFromRules(
      const ContentSettingsForOneType& rules,
      const blink::WebFrame* frame,
      const GURL& secondary_url);
  ContentSetting GetContentSettingFromRules(
      const ContentSettingsForOneType& rules,
      const blink::WebFrame* frame,
      const blink::WebURL& secondary_url);

 protected:
  // Allow this to be overridden by tests.
  virtual void BindContentSettingsManager(
      mojo::Remote<mojom::ContentSettingsManager>* manager);

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentSettingsAgentImplBrowserTest,
                           WhitelistedSchemes);
  FRIEND_TEST_ALL_PREFIXES(ContentSettingsAgentImplBrowserTest,
                           ContentSettingsInterstitialPages);

  // RenderFrameObserver implementation.
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void OnDestruct() override;

  // mojom::ContentSettingsAgent:
  void SetAllowRunningInsecureContent() override;
  void SetAsInterstitial() override;
  void SetDisabledMixedContentUpgrades() override;

  void OnContentSettingsAgentRequest(
      mojo::PendingAssociatedReceiver<mojom::ContentSettingsAgent> receiver);

  // Resets the |content_blocked_| array.
  void ClearBlockedContentSettings();

  // Helpers.
  // True if |render_frame()| contains content that is white-listed for content
  // settings.
  bool IsWhitelistedForContentSettings() const;

  // A getter for |content_settings_manager_| that ensures it is bound.
  mojom::ContentSettingsManager& GetContentSettingsManager();

  mojo::Remote<mojom::ContentSettingsManager> content_settings_manager_;

  // Insecure content may be permitted for the duration of this render view.
  bool allow_running_insecure_content_ = false;

  // A pointer to content setting rules stored by the renderer. Normally, the
  // |RendererContentSettingRules| object is owned by
  // |ChromeRenderThreadObserver|. In the tests it is owned by the caller of
  // |SetContentSettingRules|.
  const RendererContentSettingRules* content_setting_rules_ = nullptr;

  // Stores if images, scripts, and plugins have actually been blocked.
  base::flat_set<ContentSettingsType> content_blocked_;

  // Caches the result of AllowStorageAccess.
  using StoragePermissionsKey = std::pair<url::Origin, StorageType>;
  base::flat_map<StoragePermissionsKey, bool> cached_storage_permissions_;

  // Caches the result of AllowScript.
  base::flat_map<blink::WebFrame*, bool> cached_script_permissions_;

  bool is_interstitial_page_ = false;
  bool mixed_content_autoupgrades_disabled_ = false;

  // If true, IsWhitelistedForContentSettings will always return true.
  const bool should_whitelist_;

  std::unique_ptr<Delegate> delegate_;

  mojo::AssociatedReceiverSet<mojom::ContentSettingsAgent> receivers_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsAgentImpl);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_RENDERER_CONTENT_SETTINGS_AGENT_IMPL_H_
