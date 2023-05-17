// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"

#include "base/check.h"
#include "base/check_op.h"

SidePanelEntryKey::SidePanelEntryKey(SidePanelEntryId id) : id_(id) {
  CHECK_NE(id_, SidePanelEntryId::kExtension);
}

SidePanelEntryKey::SidePanelEntryKey(SidePanelEntryId id,
                                     extensions::ExtensionId extension_id)
    : id_(id), extension_id_(extension_id) {
  CHECK_EQ(id_, SidePanelEntryId::kExtension);
}

SidePanelEntryKey::SidePanelEntryKey(const SidePanelEntryKey& other) = default;

SidePanelEntryKey::~SidePanelEntryKey() = default;

SidePanelEntryKey& SidePanelEntryKey::operator=(
    const SidePanelEntryKey& other) = default;

bool SidePanelEntryKey::operator==(const SidePanelEntryKey& other) const {
  if (id_ == other.id_) {
    if (id_ == SidePanelEntryId::kExtension) {
      CHECK(extension_id_.has_value() && other.extension_id_.has_value());
      return extension_id_.value() == other.extension_id_.value();
    }
    return true;
  }
  return false;
}

bool SidePanelEntryKey::operator<(const SidePanelEntryKey& other) const {
  if (id_ == other.id_ && id_ == SidePanelEntryId::kExtension) {
    CHECK(extension_id_.has_value() && other.extension_id_.has_value());
    // TODO(corising): Updating extension sorting
    return extension_id_.value() < other.extension_id_.value();
  }
  return id_ < other.id_;
}
