// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_REPORTING_SERVICE_BASE_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_REPORTING_SERVICE_BASE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/clipboard_types.h"

namespace data_controls {

class Verdict;

// Generic interface for a helper reporting service for Data Controls events.
class ReportingServiceBase : public KeyedService {
 public:
  ~ReportingServiceBase() override = default;

  virtual void ReportPaste(const content::ClipboardEndpoint& source,
                           const content::ClipboardEndpoint& destination,
                           const content::ClipboardMetadata& metadata,
                           const Verdict& verdict) = 0;
  virtual void ReportPasteWarningBypassed(
      const content::ClipboardEndpoint& source,
      const content::ClipboardEndpoint& destination,
      const content::ClipboardMetadata& metadata,
      const Verdict& verdict) = 0;
  virtual void ReportCopy(const content::ClipboardEndpoint& source,
                          const content::ClipboardMetadata& metadata,
                          const Verdict& verdict) = 0;
  virtual void ReportCopyWarningBypassed(
      const content::ClipboardEndpoint& source,
      const content::ClipboardMetadata& metadata,
      const Verdict& verdict) = 0;

 protected:
  ReportingServiceBase() = default;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_REPORTING_SERVICE_BASE_H_
