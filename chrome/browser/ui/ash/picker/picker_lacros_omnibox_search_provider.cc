// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_lacros_omnibox_search_provider.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_lacros_provider.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/search_controller_ash.h"
#include "chrome/browser/ash/crosapi/search_controller_factory_ash.h"

PickerLacrosOmniboxSearchProvider::PickerLacrosOmniboxSearchProvider(
    crosapi::SearchControllerFactoryAsh* factory,
    bool bookmarks,
    bool history,
    bool open_tabs)
    : bookmarks_(bookmarks), history_(history), open_tabs_(open_tabs) {
  obs_.Observe(factory);
}

PickerLacrosOmniboxSearchProvider::~PickerLacrosOmniboxSearchProvider() =
    default;

crosapi::SearchControllerAsh*
PickerLacrosOmniboxSearchProvider::GetController() {
  return controller_.get();
}

void PickerLacrosOmniboxSearchProvider::OnSearchControllerFactoryBound(
    crosapi::SearchControllerFactoryAsh* factory) {
  controller_ =
      factory->CreateSearchControllerPicker(bookmarks_, history_, open_tabs_);
}

app_list::OmniboxLacrosProvider::SearchControllerCallback
PickerLacrosOmniboxSearchProvider::CreateControllerCallback(bool bookmarks,
                                                            bool history,
                                                            bool open_tabs) {
  // The following dereferences are safe, because `CrosapiManager::Get()`
  // `DCHECK`s the returned pointer, and both `CrosapiManager::crosapi_ash()`
  // and `CrosapiAsh::search_controller_factory_ash()` return a pointer to a
  // `std::unique_ptr`, which are initialised when the classes are constructed
  // and never reset.
  crosapi::SearchControllerFactoryAsh* factory =
      crosapi::CrosapiManager::Get()
          ->crosapi_ash()
          ->search_controller_factory_ash();
  auto provider = std::make_unique<PickerLacrosOmniboxSearchProvider>(
      factory, bookmarks, history, open_tabs);
  return base::BindRepeating(
      [](const std::unique_ptr<PickerLacrosOmniboxSearchProvider>& provider) {
        return provider->GetController();
      },
      std::move(provider));
}
