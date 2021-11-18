// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_REWRITE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_
#define COMPONENTS_URL_REWRITE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/url_rewrite/common/url_request_rewrite_rules.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace url_rewrite {

// Adapts the UrlRequestRewrite FIDL API to be sent to the renderers over the
// over the UrlRequestRewrite Mojo API.
class UrlRequestRewriteRulesManager final
    : public content::WebContentsObserver {
 public:
  static std::unique_ptr<UrlRequestRewriteRulesManager> CreateForTesting();

  explicit UrlRequestRewriteRulesManager(content::WebContents* web_contents);

  UrlRequestRewriteRulesManager(const UrlRequestRewriteRulesManager&) = delete;
  UrlRequestRewriteRulesManager& operator=(
      const UrlRequestRewriteRulesManager&) = delete;

  ~UrlRequestRewriteRulesManager() override;

  // Signals |rules| have been updated. Returns true if rules have been
  // successfully validated and updated, false otherwise.
  bool OnRulesUpdated(mojom::UrlRequestRewriteRulesPtr rules);

  scoped_refptr<UrlRequestRewriteRules>& GetCachedRules();

 private:
  // Test-only constructor.
  UrlRequestRewriteRulesManager();

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  scoped_refptr<UrlRequestRewriteRules> cached_rules_;

  // Map of GlobalRoutingID to their current associated remote.
  std::map<content::GlobalRenderFrameHostId,
           mojo::AssociatedRemote<mojom::UrlRequestRulesReceiver>>
      active_remotes_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace url_rewrite

#endif  // COMPONENTS_URL_REWRITE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_
