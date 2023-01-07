// Copyright 2019 The Chromium Authors
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
class UrlRequestRewriteRulesManager {
 public:
  UrlRequestRewriteRulesManager();
  UrlRequestRewriteRulesManager(const UrlRequestRewriteRulesManager&) = delete;
  UrlRequestRewriteRulesManager& operator=(
      const UrlRequestRewriteRulesManager&) = delete;
  ~UrlRequestRewriteRulesManager();

  // Observes the frames created in the |web_contents| and updates their URL
  // request rewrite rules. Returns true on success, false otherwise.
  bool AddWebContents(content::WebContents* web_contents);

  // Removes the |web_contents| from the observer list. Returns true on success,
  // false otherwise.
  bool RemoveWebContents(content::WebContents* web_contents);

  // Signals |rules| have been updated. Returns true if
  // rules have been successfully validated and updated, false otherwise.
  bool OnRulesUpdated(mojom::UrlRequestRewriteRulesPtr rules);

  const scoped_refptr<UrlRequestRewriteRules>& GetCachedRules() const;

  // Used for testing.
  size_t GetUpdatersSizeForTesting() const;

 private:
  // Observes render frame creation and destruction for a certain WebContents
  // and updates the URL request rewrite rules for each frame.
  class Updater final : public content::WebContentsObserver {
   public:
    Updater(content::WebContents* web_contents,
            const scoped_refptr<UrlRequestRewriteRules>& cached_rules);
    Updater(const Updater&) = delete;
    Updater& operator=(const Updater&) = delete;
    ~Updater() override;

    // Notifies UrlRequestRulesReceivers that URL rewrite rules are updated.
    void OnRulesUpdated(
        const scoped_refptr<UrlRequestRewriteRules>& cached_rules);

   private:
    // Callback used to iterate over the initial set of RenderFrameHosts in the
    // WebContents.
    void MaybeRegisterExistingRenderFrame(
        content::RenderFrameHost* render_frame_host);

    // content::WebContentsObserver implementation.
    void RenderFrameCreated(
        content::RenderFrameHost* render_frame_host) override;
    void RenderFrameDeleted(
        content::RenderFrameHost* render_frame_host) override;

    scoped_refptr<UrlRequestRewriteRules> cached_rules_;

    // Map of GlobalRoutingID to their current associated remote.
    std::map<content::GlobalRenderFrameHostId,
             mojo::AssociatedRemote<mojom::UrlRequestRulesReceiver>>
        active_remotes_;
    SEQUENCE_CHECKER(sequence_checker_);
  };

  scoped_refptr<UrlRequestRewriteRules> cached_rules_;

  // Map of WebContent to their URL request rewrite rules updater.
  std::map<content::WebContents*, std::unique_ptr<Updater>> updaters_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace url_rewrite

#endif  // COMPONENTS_URL_REWRITE_BROWSER_URL_REQUEST_REWRITE_RULES_MANAGER_H_
