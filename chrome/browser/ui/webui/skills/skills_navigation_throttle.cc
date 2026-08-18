// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_navigation_throttle.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/common/webui_url_constants.h"
#include "components/skills/features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace {

bool IsTriggeredOnSkillsSettingsLink(content::NavigationHandle& handle) {
  if (!ui::PageTransitionCoreTypeIs(handle.GetPageTransition(),
                                    ui::PAGE_TRANSITION_LINK)) {
    return false;
  }

  const GURL& url = handle.GetURL();
  GURL expected_url(features::kInterceptedSkillsUrl.Get());

  if (!url.EqualsIgnoringRef(expected_url)) {
    return false;
  }

  const std::optional<url::Origin>& initiator = handle.GetInitiatorOrigin();
  if (!initiator.has_value()) {
    return false;
  }

  // Navigations initiated from the web client (e.g. clients5.google.com).
  if (*initiator == url::Origin::Create(expected_url)) {
    return true;
  }

  // Navigations initiated from internal WebUI (chrome://skills).
  return initiator->host() == chrome::kChromeUISkillsHost &&
         (initiator->scheme() == content::kChromeUIScheme ||
          initiator->scheme() == content::kChromeUIUntrustedScheme);
}

}  // namespace

SkillsNavigationThrottle::SkillsNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

SkillsNavigationThrottle::~SkillsNavigationThrottle() = default;

// static
void SkillsNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(features::kSkillsWebViewV2Enabled)) {
    return;
  }

  auto& handle = registry.GetNavigationHandle();
  if (handle.IsInMainFrame() && IsTriggeredOnSkillsSettingsLink(handle)) {
    registry.AddThrottle(std::make_unique<SkillsNavigationThrottle>(registry));
  }
}

content::NavigationThrottle::ThrottleCheckResult
SkillsNavigationThrottle::WillStartRequest() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return content::NavigationThrottle::PROCEED;
  }

  // If the navigation is in a guest (webview), navigate the host WebContents.
  if (web_contents->GetOuterWebContents()) {
    web_contents = web_contents->GetOuterWebContents();
  }

  // Open `chrome://settings/ai/skills` asynchronously to avoid crashes due to
  // nested navigations.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> web_contents) {
            if (web_contents) {
              // If this is a new tab (initial navigation), reuse it. Otherwise,
              // spawn a new tab.
              const bool is_initial_navigation =
                  web_contents->GetController().IsInitialNavigation();

              const WindowOpenDisposition disposition =
                  is_initial_navigation
                      ? WindowOpenDisposition::CURRENT_TAB
                      : WindowOpenDisposition::NEW_FOREGROUND_TAB;

              content::OpenURLParams params(
                  GURL(features::kSkillsSettingsPageUrl.Get()),
                  content::Referrer(), disposition, ui::PAGE_TRANSITION_LINK,
                  /*is_renderer_initiated=*/false);
              web_contents->OpenURL(params,
                                    /*navigation_handle_callback=*/{});
            }
          },
          web_contents->GetWeakPtr()));

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

const char* SkillsNavigationThrottle::GetNameForLogging() {
  return "SkillsNavigationThrottle";
}
