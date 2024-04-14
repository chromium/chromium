// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PICKER_PICKER_LACROS_OMNIBOX_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_PICKER_PICKER_LACROS_OMNIBOX_SEARCH_PROVIDER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_lacros_provider.h"
#include "chrome/browser/ash/crosapi/search_controller_factory_ash.h"

namespace crosapi {
class SearchControllerAsh;
}

// Manages a dedicated Picker `crosapi::SearchControllerAsh` obtained from a
// given `crosapi::SearchControllerFactoryAsh`.
// Intended to be used to construct a `app_list::OmniboxLacrosProvider` - see
// the `CreateControllerCallback` static method below.
class PickerLacrosOmniboxSearchProvider
    : public crosapi::SearchControllerFactoryAsh::Observer {
 public:
  explicit PickerLacrosOmniboxSearchProvider(
      crosapi::SearchControllerFactoryAsh* factory,
      bool bookmarks,
      bool history,
      bool open_tabs);
  PickerLacrosOmniboxSearchProvider(const PickerLacrosOmniboxSearchProvider&) =
      delete;
  PickerLacrosOmniboxSearchProvider& operator=(
      const PickerLacrosOmniboxSearchProvider&) = delete;
  ~PickerLacrosOmniboxSearchProvider() override;

  crosapi::SearchControllerAsh* GetController();

  // Returns a `SearchControllerCallback` for use with
  // `app_list::OmniboxLacrosProvider` which uses the singleton
  // `crosapi::SearchControllerFactoryAsh` to create a dedicated search
  // controller for Picker.
  static app_list::OmniboxLacrosProvider::SearchControllerCallback
  CreateControllerCallback(bool bookmarks, bool history, bool open_tabs);

 private:
  // crosapi::SearchControllerFactoryAsh::Observer overrides:
  void OnSearchControllerFactoryBound(
      crosapi::SearchControllerFactoryAsh* factory) override;

  bool bookmarks_;
  bool history_;
  bool open_tabs_;

  std::unique_ptr<crosapi::SearchControllerAsh> controller_;

  base::ScopedObservation<crosapi::SearchControllerFactoryAsh,
                          crosapi::SearchControllerFactoryAsh::Observer>
      obs_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PICKER_PICKER_LACROS_OMNIBOX_SEARCH_PROVIDER_H_
