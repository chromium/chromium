// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PROPERTIES_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PROPERTIES_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "ui/actions/action_id.h"
#include "ui/base/interaction/element_identifier.h"

namespace page_actions {

// Defines the static properties that a page action can have. The page action in
// mainly configured using the ActionItem. But the ActionItem is global.
// Therefore, for some properties, they should be scoped to page actions only.
struct PageActionProperties {
  // Indicates the metric name used for the page action during reporting. This
  // is mandatory.
  const char* histogram_name = nullptr;
  // Indicates whether the page action is always visible or will be
  // conditionally visible for some time. This is optional.
  // Defaulted to `true` because most page actions are be ephemeral.
  bool is_ephemeral = true;
  // This allows the page action to be exempt from the controller-wide
  // suppression due to current omnibox state (eg. Omnibox popup open, or
  // Omnibox text being edited). This is optional.
  bool exempt_from_omnibox_suppression = false;
  // Indicates the page action type and it's mandatory.
  PageActionIconType type;
  // This indicates the page action view element identifier. This is optional.
  ui::ElementIdentifier element_identifier;
};

using PageActionPropertiesMap =
    base::flat_map<actions::ActionId, PageActionProperties>;

class PageActionPropertiesProviderInterface {
 public:
  virtual ~PageActionPropertiesProviderInterface() = default;

  virtual const PageActionProperties& GetProperties(
      actions::ActionId action_id) const = 0;
};

class PageActionPropertiesProvider
    : public PageActionPropertiesProviderInterface {
 public:
  PageActionPropertiesProvider();
  ~PageActionPropertiesProvider() override;

  bool Contains(actions::ActionId action_id) const;

  // PageActionPropertiesProviderInterface
  const PageActionProperties& GetProperties(
      actions::ActionId action_id) const final;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PROPERTIES_PROVIDER_H_
