// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/open_tab_provider.h"

OpenTabProvider::OpenTabProvider()
    : AutocompleteProvider(AutocompleteProvider::TYPE_OPEN_TAB) {}

OpenTabProvider::~OpenTabProvider() = default;

void OpenTabProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  // TODO(crbug.com/1293702): WIP.
}
