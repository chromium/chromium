// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_TEST_PAGE_ACTION_PROPERTIES_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_TEST_PAGE_ACTION_PROPERTIES_PROVIDER_H_

#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "ui/actions/action_id.h"

namespace page_actions {

// Testing provider that will tests to configure a custom list of properties.
class TestPageActionPropertiesProvider
    : public PageActionPropertiesProviderInterface {
 public:
  explicit TestPageActionPropertiesProvider(
      const PageActionPropertiesMap& properties);
  ~TestPageActionPropertiesProvider() override;

  // PageActionPropertiesProviderInterface
  const PageActionProperties& GetProperties(
      actions::ActionId action_id) const final;

 private:
  const PageActionPropertiesMap properties_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_TEST_PAGE_ACTION_PROPERTIES_PROVIDER_H_
