// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_REQUEST_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_REQUEST_FACTORY_H_

#include <memory>

class TabOrganizationRequest;

class TabOrganizationRequestFactory {
 public:
  virtual ~TabOrganizationRequestFactory();
  virtual std::unique_ptr<TabOrganizationRequest> CreateRequest() = 0;
};

class TwoTabsRequestFactory : public TabOrganizationRequestFactory {
 public:
  ~TwoTabsRequestFactory() override;
  std::unique_ptr<TabOrganizationRequest> CreateRequest() override;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_REQUEST_FACTORY_H_
