// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_REPORTING_SERVICE_BASE_FACTORY_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_REPORTING_SERVICE_BASE_FACTORY_H_

namespace content {
class BrowserContext;
}  // namespace content

namespace data_controls {

class ReportingServiceBase;

class ReportingServiceBaseFactory {
 public:
  virtual ReportingServiceBase* GetForBrowserContext(
      content::BrowserContext* browser_context) = 0;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_REPORTING_SERVICE_BASE_FACTORY_H_
