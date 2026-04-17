// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/experimental_injector.h"

namespace tabs_api::testing {

ExperimentalInjector::ExperimentalInjector()
    : context_menu_adapter_(std::make_unique<ToyTabContextMenuAdapter>()) {}

ExperimentalInjector::~ExperimentalInjector() = default;

}  // namespace tabs_api::testing
