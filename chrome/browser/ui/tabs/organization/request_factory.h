// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_REQUEST_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_REQUEST_FACTORY_H_

#include <memory>

class TabOrganizationRequest;
class Profile;

class TabOrganizationRequestFactory {
 public:
  virtual ~TabOrganizationRequestFactory();
  virtual std::unique_ptr<TabOrganizationRequest> CreateRequest(
      Profile* profile) = 0;

  static std::unique_ptr<TabOrganizationRequestFactory> GetForProfile(
      Profile* profile);
};

class TwoTabsRequestFactory : public TabOrganizationRequestFactory {
 public:
  ~TwoTabsRequestFactory() override;
  std::unique_ptr<TabOrganizationRequest> CreateRequest(
      Profile* profile) override;
};

class OptimizationGuideTabOrganizationRequestFactory
    : public TabOrganizationRequestFactory {
 public:
  ~OptimizationGuideTabOrganizationRequestFactory() override;
  std::unique_ptr<TabOrganizationRequest> CreateRequest(
      Profile* profile) override;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_REQUEST_FACTORY_H_
