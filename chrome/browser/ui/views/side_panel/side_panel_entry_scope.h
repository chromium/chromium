// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_SCOPE_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_SCOPE_H_

#include <utility>

class BrowserWindowInterface;

namespace tabs {
class TabInterface;
}  // namespace tabs

// Scope of a SidePanelEntry. Provides access to the appropriate browser and tab
// interface.
class SidePanelEntryScope {
 public:
  enum class ScopeType {
    kBrowser,
    kTab,
  };

  explicit SidePanelEntryScope(ScopeType scope_type);
  SidePanelEntryScope(const SidePanelEntryScope&) = delete;
  SidePanelEntryScope& operator=(const SidePanelEntryScope&) = delete;
  virtual ~SidePanelEntryScope() = default;

  ScopeType get_scope_type() const { return scope_type_; }

  // Returns a tab interface for tab. This is valid for tab scoped entries only
  // and CHECK enforced when called.
  // Note: When overriding `GetTabInterface()` clients should be aware that
  // the returned object can have its const-ness cast away.
  tabs::TabInterface& GetTabInterface() {
    return const_cast<tabs::TabInterface&>(
        std::as_const(*this).GetTabInterface());
  }
  virtual const tabs::TabInterface& GetTabInterface() const = 0;

  // Returns a valid browser interface for either browser or tab scoped entries.
  // Note: When overriding `GetBrowserWindowInterface()` clients should be aware
  // that the returned object can have its const-ness cast away.
  BrowserWindowInterface& GetBrowserWindowInterface() {
    return const_cast<BrowserWindowInterface&>(
        std::as_const(*this).GetBrowserWindowInterface());
  }
  virtual const BrowserWindowInterface& GetBrowserWindowInterface() const = 0;

 private:
  const ScopeType scope_type_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_SCOPE_H_
