// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_container.h"

#include "ui/base/metadata/metadata_impl_macros.h"

TabContainer::TabInsertionParams::TabInsertionParams(std::unique_ptr<Tab> tab,
                                                     int index,
                                                     TabPinned pinned)
    : tab(std::move(tab)), model_index(index), pinned(pinned) {}

TabContainer::TabInsertionParams::~TabInsertionParams() = default;

TabContainer::TabInsertionParams::TabInsertionParams(
    TabInsertionParams&& other) noexcept
    : tab(std::move(other.tab)),
      model_index(other.model_index),
      pinned(other.pinned) {}

TabContainer::TabInsertionParams& TabContainer::TabInsertionParams::operator=(
    TabInsertionParams&& other) noexcept {
  if (this != &other) {
    tab = std::move(other.tab);
    model_index = other.model_index;
    pinned = other.pinned;
  }
  return *this;
}

BEGIN_METADATA(TabContainer)
END_METADATA
