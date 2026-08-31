// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEVTOOLS_CAST_DEVTOOLS_MANAGER_DELEGATE_H_
#define CHROMECAST_BROWSER_DEVTOOLS_CAST_DEVTOOLS_MANAGER_DELEGATE_H_

#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace chromecast {
namespace shell {

// Implements a whitelist of WebContents allowed for remote debugging.
class CastDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  // TODO(derekjchow): Remove use of GetInstance.
  static CastDevToolsManagerDelegate* GetInstance();

  CastDevToolsManagerDelegate();

  CastDevToolsManagerDelegate(const CastDevToolsManagerDelegate&) = delete;
  CastDevToolsManagerDelegate& operator=(const CastDevToolsManagerDelegate&) =
      delete;

  ~CastDevToolsManagerDelegate() override;

  void EnableWebContentsForDebugging(content::WebContents* web_contents);
  void DisableWebContentsForDebugging(content::WebContents* web_contents);
  bool HasEnabledWebContents() const;

  // content::DevToolsManagerDelegate implementation.
  content::DevToolsAgentHost::List RemoteDebuggingTargets(
      TargetType target_type) override;
  std::string GetDiscoveryPageHTML() override;
  bool AllowInspectingRenderFrameHost(content::RenderFrameHost* rfh) override;

 private:
  std::unordered_set<raw_ptr<content::WebContents, DanglingUntriaged>>
      enabled_webcontents_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DEVTOOLS_CAST_DEVTOOLS_MANAGER_DELEGATE_H_
