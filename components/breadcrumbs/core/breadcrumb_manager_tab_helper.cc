// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager_tab_helper.h"

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "net/base/net_errors.h"

namespace breadcrumbs {

namespace {

// Returns true if navigation URL host is google.com or www.google.com.
bool IsGoogleUrl(const GURL& url) {
  return url.host() == "google.com" || url.host() == "www.google.com";
}

}  // namespace

const char kBreadcrumbDidStartNavigation[] = "StartNav";
const char kBreadcrumbDidFinishNavigation[] = "FinishNav";
const char kBreadcrumbPageLoaded[] = "PageLoad";
const char kBreadcrumbDidChangeVisibleSecurityState[] = "SecurityChange";

const char kBreadcrumbInfobarAdded[] = "AddInfobar";
const char kBreadcrumbInfobarRemoved[] = "RemoveInfobar";
const char kBreadcrumbInfobarReplaced[] = "ReplaceInfobar";

const char kBreadcrumbScroll[] = "Scroll";
const char kBreadcrumbZoom[] = "Zoom";

const char kBreadcrumbAuthenticationBroken[] = "#broken";
const char kBreadcrumbDownload[] = "#download";
const char kBreadcrumbMixedContent[] = "#mixed";
const char kBreadcrumbInfobarNotAnimated[] = "#not-animated";
const char kBreadcrumbNtpNavigation[] = "#ntp";
const char kBreadcrumbGoogleNavigation[] = "#google";
const char kBreadcrumbPdfLoad[] = "#pdf";
const char kBreadcrumbPageLoadFailure[] = "#failure";
const char kBreadcrumbRendererInitiatedByUser[] = "#renderer-user";
const char kBreadcrumbRendererInitiatedByScript[] = "#renderer-script";

BreadcrumbManagerTabHelper::BreadcrumbManagerTabHelper(
    infobars::InfoBarManager* infobar_manager) {
  static int next_unique_id = 1;
  unique_id_ = next_unique_id++;

  infobar_manager_ = infobar_manager;
  infobar_observation_.Observe(infobar_manager_.get());
}

BreadcrumbManagerTabHelper::~BreadcrumbManagerTabHelper() = default;

void BreadcrumbManagerTabHelper::LogDidStartNavigation(
    int64_t navigation_id,
    GURL url,
    bool is_ntp_url,
    bool is_renderer_initiated,
    bool has_user_gesture,
    ui::PageTransition page_transition) {
  std::vector<std::string> event = {
      base::StringPrintf("%s%" PRIu64, kBreadcrumbDidStartNavigation,
                         navigation_id),
  };

  if (is_ntp_url) {
    event.push_back(kBreadcrumbNtpNavigation);
  } else if (IsGoogleUrl(url)) {
    event.push_back(kBreadcrumbGoogleNavigation);
  }

  if (is_renderer_initiated) {
    if (has_user_gesture) {
      event.push_back(kBreadcrumbRendererInitiatedByUser);
    } else {
      event.push_back(kBreadcrumbRendererInitiatedByScript);
    }
  }

  event.push_back(base::StringPrintf(
      "#%s", ui::PageTransitionGetCoreTransitionString(page_transition)));

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::LogDidFinishNavigation(int64_t navigation_id,
                                                        bool is_download,
                                                        int error_code) {
  std::vector<std::string> event = {
      base::StringPrintf("%s%" PRIu64,
                         breadcrumbs::kBreadcrumbDidFinishNavigation,
                         navigation_id),
  };
  if (is_download)
    event.push_back(breadcrumbs::kBreadcrumbDownload);
  if (error_code)
    event.push_back(net::ErrorToShortString(error_code));
  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::LogPageLoaded(
    bool is_ntp_url,
    GURL url,
    bool page_load_success,
    const std::string& contents_mime_type) {
  std::vector<std::string> event = {breadcrumbs::kBreadcrumbPageLoaded};

  if (is_ntp_url) {
    // NTP load can't fail, so there is no need to report success/failure.
    event.push_back(breadcrumbs::kBreadcrumbNtpNavigation);
  } else {
    if (IsGoogleUrl(url)) {
      event.push_back(breadcrumbs::kBreadcrumbGoogleNavigation);
    }

    if (page_load_success) {
      if (contents_mime_type == "application/pdf") {
        event.push_back(breadcrumbs::kBreadcrumbPdfLoad);
      }
    } else {
      event.push_back(breadcrumbs::kBreadcrumbPageLoadFailure);
    }
  }

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::LogDidChangeVisibleSecurityState(
    bool displayed_mixed_content,
    bool security_style_authentication_broken) {
  std::vector<std::string> event;
  if (displayed_mixed_content)
    event.push_back(breadcrumbs::kBreadcrumbMixedContent);
  if (security_style_authentication_broken)
    event.push_back(breadcrumbs::kBreadcrumbAuthenticationBroken);

  if (!event.empty()) {
    event.insert(event.begin(),
                 breadcrumbs::kBreadcrumbDidChangeVisibleSecurityState);
    LogEvent(base::JoinString(event, " "));
  }
}

void BreadcrumbManagerTabHelper::LogRenderProcessGone() {
  LogEvent("RenderProcessGone");
}

void BreadcrumbManagerTabHelper::LogEvent(const std::string& event) {
  PlatformLogEvent(
      base::StringPrintf("Tab%d %s", GetUniqueId(), event.c_str()));
}

bool BreadcrumbManagerTabHelper::ShouldLogRepeatedEvent(int count) {
  return count == 1 || count == 2 || count == 5 || count == 20 ||
         count == 100 || count == 200;
}

void BreadcrumbManagerTabHelper::OnInfoBarAdded(infobars::InfoBar* infobar) {
  sequentially_replaced_infobars_ = 0;
  LogEvent(base::StringPrintf("%s%d", kBreadcrumbInfobarAdded,
                              infobar->GetIdentifier()));
}

void BreadcrumbManagerTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                  bool animate) {
  sequentially_replaced_infobars_ = 0;
  std::vector<std::string> event = {
      base::StringPrintf("%s%d", kBreadcrumbInfobarRemoved,
                         infobar->GetIdentifier()),
  };
  if (!animate)
    event.push_back(kBreadcrumbInfobarNotAnimated);
  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::OnInfoBarReplaced(
    infobars::InfoBar* old_infobar,
    infobars::InfoBar* new_infobar) {
  sequentially_replaced_infobars_++;

  if (ShouldLogRepeatedEvent(sequentially_replaced_infobars_)) {
    LogEvent(base::StringPrintf("%s%d %d", kBreadcrumbInfobarReplaced,
                                new_infobar->GetIdentifier(),
                                sequentially_replaced_infobars_));
  }
}

void BreadcrumbManagerTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK_EQ(infobar_manager_, manager);
  DCHECK(infobar_observation_.IsObservingSource(manager));
  infobar_observation_.Reset();
  infobar_manager_ = nullptr;
  sequentially_replaced_infobars_ = 0;
}

}  // namespace breadcrumbs