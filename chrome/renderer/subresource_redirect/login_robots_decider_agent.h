// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_AGENT_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_AGENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/renderer/subresource_redirect/public_resource_decider_agent.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_redirect {

// The decider agent implementation that allows subresource redirect compression
// based on robots rules on non-logged-in pages. Currently only handles
// mainframes.
// TODO(crbug.com/1149853): Add the logged-in checks.
class LoginRobotsDeciderAgent : public PublicResourceDeciderAgent {
 public:
  LoginRobotsDeciderAgent(
      blink::AssociatedInterfaceRegistry* associated_interfaces,
      content::RenderFrame* render_frame);
  ~LoginRobotsDeciderAgent() override;

  LoginRobotsDeciderAgent(const LoginRobotsDeciderAgent&) = delete;
  LoginRobotsDeciderAgent& operator=(const LoginRobotsDeciderAgent&) = delete;

 private:
  friend class SubresourceRedirectLoginRobotsDeciderAgentTest;
  friend class SubresourceRedirectLoginRobotsURLLoaderThrottleTest;

  // content::RenderFrameObserver:
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void PreloadSubresourceOptimizationsForOrigins(
      const std::vector<blink::WebSecurityOrigin>& origins) override;

  // mojom::SubresourceRedirectHintsReceiver:
  void SetCompressPublicImagesHints(
      mojom::CompressPublicImagesHintsPtr images_hints) override;
  void SetLoggedInState(bool is_logged_in) override;

  // PublicResourceDeciderAgent:
  absl::optional<SubresourceRedirectResult> ShouldRedirectSubresource(
      const GURL& url,
      ShouldRedirectDecisionCallback callback) override;
  void RecordMetricsOnLoadFinished(
      const GURL& url,
      int64_t content_length,
      SubresourceRedirectResult redirect_result) override;
  void NotifyIneligibleBlinkDisallowedSubresource() override;

  // Callback invoked when should redirect check result is available.
  void OnShouldRedirectSubresourceResult(
      ShouldRedirectDecisionCallback callback,
      RobotsRulesParser::CheckResult check_result);

  bool IsMainFrame() const;

  // Creates and starts the fetch of robots rules for |origin| if the rules are
  // not available. |rules_receive_timeout| is the timeout value for receiving
  // the fetched rules.
  void CreateAndFetchRobotsRules(const url::Origin& origin,
                                 const base::TimeDelta& rules_receive_timeout);

  // Current state of the redirect compression that should be used for the
  // current navigation.
  SubresourceRedirectResult redirect_result_ =
      SubresourceRedirectResult::kUnknown;

  // Tracks the count of subresource redirect allowed checks that happened for
  // the current navigation. This is used in having a different robots rules
  // fetch timeout for the first k subresources.
  size_t num_should_redirect_checks_ = 0;

  // Saves whether the upcoming navigation is logged-in. This is updated via the
  // SetLoggedInState() mojo which is sent just before the navigation is
  // committed in the browser process, and used in ReadyToCommitNavigation()
  // when the navigation is committed in the renderer process. Value of
  // absl::nullopt means logged-in state hasn't arrived from the browser. This
  // value should be reset after each navigation commit, so that it won't get
  // accidentally reused for subsequent navigations.
  absl::optional<bool> is_pending_navigation_loggged_in_;

  THREAD_CHECKER(thread_checker_);

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<LoginRobotsDeciderAgent> weak_ptr_factory_{this};
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_AGENT_H_
