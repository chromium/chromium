// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_AGENT_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_AGENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
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

  void UpdateRobotsRulesForTesting(const url::Origin& origin,
                                   const base::Optional<std::string>& rules);

  // content::RenderFrameObserver:
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;

  // mojom::SubresourceRedirectHintsReceiver:
  void SetCompressPublicImagesHints(
      mojom::CompressPublicImagesHintsPtr images_hints) override;
  void SetLoggedInState(bool is_logged_in) override;

  // PublicResourceDeciderAgent:
  base::Optional<SubresourceRedirectResult> ShouldRedirectSubresource(
      const GURL& url,
      ShouldRedirectDecisionCallback callback) override;
  void RecordMetricsOnLoadFinished(
      const GURL& url,
      int64_t content_length,
      SubresourceRedirectResult redirect_result) override;

  // Callback invoked when should redirect check result is available.
  void OnShouldRedirectSubresourceResult(
      ShouldRedirectDecisionCallback callback,
      RobotsRulesParser::CheckResult check_result);

  bool IsMainFrame() const;

  // Current state of the redirect compression that should be used for the
  // current navigation.
  SubresourceRedirectResult redirect_result_ =
      SubresourceRedirectResult::kUnknown;

  // Tracks the count of subresource redirect allowed checks that happened for
  // the current navigation. This is used in having a different robots rules
  // fetch timeout for the first k subresources.
  size_t num_should_redirect_checks_ = 0;

  THREAD_CHECKER(thread_checker_);

  // Used to get a weak pointer to |this|.
  base::WeakPtrFactory<LoginRobotsDeciderAgent> weak_ptr_factory_{this};
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_AGENT_H_
