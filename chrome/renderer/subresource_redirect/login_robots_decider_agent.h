// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_AGENT_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_AGENT_H_

#include "base/macros.h"
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
// TODO(crbug.com/1148974): Handle subresources in subframes as well.
// TODO(crbug.com/1149853): Add the logged-in checks.
class LoginRobotsDeciderAgent : public PublicResourceDeciderAgent {
 public:
  LoginRobotsDeciderAgent(
      blink::AssociatedInterfaceRegistry* associated_interfaces,
      content::RenderFrame* render_frame);
  ~LoginRobotsDeciderAgent() override;

  LoginRobotsDeciderAgent(const LoginRobotsDeciderAgent&) = delete;
  LoginRobotsDeciderAgent& operator=(const LoginRobotsDeciderAgent&) = delete;

  // Updates the robots rules for the origin.
  void UpdateRobotsRules(const url::Origin& origin, const std::string& rules);

 private:
  friend class SubresourceRedirectLoginRobotsDeciderAgentTest;

  // mojom::SubresourceRedirectHintsReceiver:
  void SetCompressPublicImagesHints(
      mojom::CompressPublicImagesHintsPtr images_hints) override;

  // PublicResourceDeciderAgent:
  base::Optional<RedirectResult> ShouldRedirectSubresource(
      const GURL& url,
      ShouldRedirectDecisionCallback callback) override;
  void RecordMetricsOnLoadFinished(const GURL& url,
                                   int64_t content_length,
                                   RedirectResult redirect_result) override;

  bool IsMainFrame() const;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_LOGIN_ROBOTS_DECIDER_AGENT_H_
