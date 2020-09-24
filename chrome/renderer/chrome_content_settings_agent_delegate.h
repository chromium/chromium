// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_CONTENT_SETTINGS_AGENT_DELEGATE_H_
#define CHROME_RENDERER_CHROME_CONTENT_SETTINGS_AGENT_DELEGATE_H_

#include "components/content_settings/renderer/content_settings_agent_impl.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace extensions {
class Dispatcher;
class Extension;
}  // namespace extensions
#endif

class ChromeContentSettingsAgentDelegate
    : public content_settings::ContentSettingsAgentImpl::Delegate,
      public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<
          ChromeContentSettingsAgentDelegate> {
 public:
  explicit ChromeContentSettingsAgentDelegate(
      content::RenderFrame* render_frame);
  ~ChromeContentSettingsAgentDelegate() override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Sets the extension dispatcher. Call this right after constructing this
  // class. This should only be called once.
  void SetExtensionDispatcher(extensions::Dispatcher* extension_dispatcher);
#endif

  bool IsPluginTemporarilyAllowed(const std::string& identifier);

  // content_settings::ContentSettingsAgentImpl::Delegate:
  bool IsSchemeWhitelisted(const std::string& scheme) override;
  base::Optional<bool> AllowReadFromClipboard() override;
  base::Optional<bool> AllowWriteToClipboard() override;
  base::Optional<bool> AllowMutationEvents() override;
  void PassiveInsecureContentFound(const blink::WebURL&) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentSettingsAgentDelegateBrowserTest,
                           PluginsTemporarilyAllowed);

  // RenderFrameObserver:
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void OnDestruct() override;

  void OnLoadBlockedPlugins(const std::string& identifier);

  // Whether the observed RenderFrame is for a platform app.
  bool IsPlatformApp();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If |origin| corresponds to an installed extension, returns that extension.
  // Otherwise returns null.
  const extensions::Extension* GetExtension(
      const blink::WebSecurityOrigin& origin) const;

  // Owned by ChromeContentRendererClient and outlive us.
  extensions::Dispatcher* extension_dispatcher_ = nullptr;
#endif

  base::flat_set<std::string> temporarily_allowed_plugins_;

  content::RenderFrame* render_frame_ = nullptr;
};

#endif  // CHROME_RENDERER_CHROME_CONTENT_SETTINGS_AGENT_DELEGATE_H_
