// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_TAB_CONTEXT_INFO_H_
#define COMPONENTS_TTC_APP_PUBLIC_TAB_CONTEXT_INFO_H_

#include <ostream>
#include <string>

#include "url/gurl.h"

namespace ttc {

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

std::ostream& operator<<(std::ostream& os, const TabContextInfo& info);

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_TAB_CONTEXT_INFO_H_
