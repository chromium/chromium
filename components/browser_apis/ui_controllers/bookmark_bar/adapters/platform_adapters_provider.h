// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_PLATFORM_ADAPTERS_PROVIDER_H_
#define COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_PLATFORM_ADAPTERS_PROVIDER_H_

namespace bookmark_bar_api {

class ActionAdapter;
class ModelAdapter;
class PreferencesAdapter;

// Configures the bookmark bar UI controller for an embedder. The provider owns
// the adapters, and must outlive any use of them.
class PlatformAdaptersProvider {
 public:
  virtual ~PlatformAdaptersProvider() = default;

  virtual ActionAdapter& action_adapter() = 0;
  virtual ModelAdapter& model_adapter() = 0;
  virtual PreferencesAdapter& preferences_adapter() = 0;
};

}  // namespace bookmark_bar_api

#endif  // COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_PLATFORM_ADAPTERS_PROVIDER_H_
