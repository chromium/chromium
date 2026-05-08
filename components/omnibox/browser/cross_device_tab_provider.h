// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_CROSS_DEVICE_TAB_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_CROSS_DEVICE_TAB_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteInput;
class AutocompleteProviderClient;

// Autocomplete provider for tabs from other devices.
class CrossDeviceTabProvider : public AutocompleteProvider {
 public:
  explicit CrossDeviceTabProvider(AutocompleteProviderClient* client);

  CrossDeviceTabProvider(const CrossDeviceTabProvider&) = delete;
  CrossDeviceTabProvider& operator=(const CrossDeviceTabProvider&) = delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~CrossDeviceTabProvider() override;

  const raw_ptr<AutocompleteProviderClient> client_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_CROSS_DEVICE_TAB_PROVIDER_H_
