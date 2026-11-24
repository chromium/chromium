// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_PAGE_CONTEXT_TYPES_H_
#define COMPONENTS_TTC_APP_PUBLIC_PAGE_CONTEXT_TYPES_H_

#include <ostream>
#include <string>
#include <variant>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "url/gurl.h"

namespace ttc {

// Broad category explaining why page context was withheld for the active tab.
// Recorded for server diagnostics only and never included in model prompts.
enum class PageRestrictionReason {
  kOther,
  kUnsupportedScheme,
  kEnterprisePolicy,
  kSafetyPolicy,
  kRendererUnavailable,
};

// Describes the tab the session is operating on, as sent to the model
// alongside the page's content.
struct TabContextInfo {
  TabContextInfo();
  TabContextInfo(GURL url, std::string title, std::string mime_type);
  TabContextInfo(const TabContextInfo&);
  TabContextInfo& operator=(const TabContextInfo&);
  TabContextInfo(TabContextInfo&&) noexcept;
  TabContextInfo& operator=(TabContextInfo&&) noexcept;
  ~TabContextInfo();

  // The URL of the tab's page, or just its origin if the page's context may
  // not be shared.
  GURL url;

  // The title of the tab's page. Empty if the page's context may not be
  // shared.
  std::string title;

  // The MIME type of the tab's document, e.g. "text/html" or
  // "application/pdf". Empty if unknown or if the page's context may not be
  // shared.
  std::string mime_type;
};

// Context for an active tab whose metadata (and optional content) may be
// shared with the model.
struct SharedPageContext {
  SharedPageContext();
  explicit SharedPageContext(
      TabContextInfo tab,
      optimization_guide::proto::AnnotatedPageContent apc = {});
  SharedPageContext(const SharedPageContext&);
  SharedPageContext& operator=(const SharedPageContext&);
  SharedPageContext(SharedPageContext&&);
  SharedPageContext& operator=(SharedPageContext&&);
  ~SharedPageContext();

  TabContextInfo tab;
  // Empty when the content isn't available (e.g. PDFs, transient failures).
  optimization_guide::proto::AnnotatedPageContent apc;
};

// Marks the active tab as restricted: no URL, title, or content may be shared.
struct RestrictedPageContext {
  PageRestrictionReason reason = PageRestrictionReason::kOther;
};

using PageContextUpdate =
    std::variant<SharedPageContext, RestrictedPageContext>;

std::ostream& operator<<(std::ostream& os, const TabContextInfo& info);
std::ostream& operator<<(std::ostream& os, const SharedPageContext& context);
std::ostream& operator<<(std::ostream& os,
                         const RestrictedPageContext& context);

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_PAGE_CONTEXT_TYPES_H_
