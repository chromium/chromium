// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/channel_override_work_item.h"

#include <string>

#include "base/logging.h"
#include "chrome/install_static/install_util.h"

namespace {

// Returns true if `channel` is the canonical name of the current channel.
bool IsCurrentChannel(const std::wstring& channel) {
  return channel ==
         install_static::GetChromeChannelName(/*with_extended_stable=*/true);
}

}  // namespace

ChannelOverrideWorkItem::ChannelOverrideWorkItem() = default;

ChannelOverrideWorkItem::~ChannelOverrideWorkItem() = default;

bool ChannelOverrideWorkItem::DoImpl() {
  // Read the "ap" value.
  installer::AdditionalParameters ap;

  if (IsCurrentChannel(ap.ParseChannel()))
    return true;  // No modification is necessary.

  // Cache the unmodified value for use in rollback.
  original_ap_.emplace();

  // Update and persist the value.
  ap.SetChannel(install_static::GetChromeChannel(),
                install_static::IsExtendedStableChannel());
  if (ap.Commit()) {
    VLOG(1) << "Updated \"ap\" with channel override from \""
            << original_ap_->value() << "\" to \"" << ap.value() << "\"";
    return true;
  }

  PLOG(ERROR) << "Failed to update \"ap\" with channel override from \""
              << original_ap_->value() << "\" to \"" << ap.value() << "\"";
  return false;
}

void ChannelOverrideWorkItem::RollbackImpl() {
  if (original_ap_ && !original_ap_->Commit()) {
    PLOG(ERROR) << "Failed to roll back channel override to \""
                << original_ap_->value() << "\"";
  }
  original_ap_.reset();
}
