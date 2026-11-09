// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SECTION_H_

#include "base/memory/raw_ptr.h"

namespace settings_search {

class SettingsSearchRegistry;

// Base class for one settings page. Subclasses register their initial
// searchable settings in their constructors, and add or remove items as
// runtime state changes (prefs, policy, sign-in). Subclasses resolve their
// strings when building items, since SettingsSearchItem takes localized
// strings. Sections need not remove their items on destruction; the registry is
// destroyed with them.
class SettingsSection {
 public:
  // `registry` must outlive `this`.
  explicit SettingsSection(SettingsSearchRegistry* registry);
  SettingsSection(const SettingsSection&) = delete;
  SettingsSection& operator=(const SettingsSection&) = delete;
  virtual ~SettingsSection();

 protected:
  SettingsSearchRegistry* registry() const { return registry_; }

 private:
  const raw_ptr<SettingsSearchRegistry> registry_;
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SECTION_H_
