// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_filter.h"

#include "base/check_is_test.h"

namespace web_app {

WebAppFilter::WebAppFilter() = default;
WebAppFilter::WebAppFilter(const WebAppFilter&) = default;

// static
WebAppFilter WebAppFilter::OpensInBrowserTab() {
  WebAppFilter filter;
  filter.opens_in_browser_tab_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::OpensInDedicatedWindow() {
  WebAppFilter filter;
  filter.opens_in_dedicated_window_ = true;
  return filter;
}

// static
#if !BUILDFLAG(IS_CHROMEOS)
WebAppFilter WebAppFilter::CapturesLinksInScope() {
  WebAppFilter filter;
  filter.captures_links_in_scope_ = true;
  return filter;
}
#endif

// static
WebAppFilter WebAppFilter::IsIsolatedApp() {
  WebAppFilter filter;
  filter.is_isolated_app_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsCraftedApp() {
  WebAppFilter filter;
  filter.is_crafted_app_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsSuggestedApp() {
  WebAppFilter filter;
  filter.is_suggested_app_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::DisplaysBadgeOnOs() {
  WebAppFilter filter;
  filter.displays_badge_on_os_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::SupportsOsNotifications() {
  WebAppFilter filter;
  filter.supports_os_notifications_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::InstalledInChrome() {
  WebAppFilter filter;
  filter.installed_in_chrome_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::InstalledInOperatingSystemForTesting() {
  CHECK_IS_TEST();
  WebAppFilter filter;
  filter.installed_in_os_ = true;
  return filter;
}

// static
WebAppFilter WebAppFilter::IsDiyWithOsShortcut() {
  WebAppFilter filter;
  filter.is_diy_with_os_shortcut_ = true;
  return filter;
}

}  // namespace web_app
