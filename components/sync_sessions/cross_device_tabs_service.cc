// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/cross_device_tabs_service.h"

namespace sync_sessions {

CrossDeviceTabsService::RemoteDeviceTabsEntry::RemoteDeviceTabsEntry() =
    default;
CrossDeviceTabsService::RemoteDeviceTabsEntry::RemoteDeviceTabsEntry(
    const RemoteDeviceTabsEntry&) = default;
CrossDeviceTabsService::RemoteDeviceTabsEntry&
CrossDeviceTabsService::RemoteDeviceTabsEntry::operator=(
    const RemoteDeviceTabsEntry&) = default;
CrossDeviceTabsService::RemoteDeviceTabsEntry::RemoteDeviceTabsEntry(
    RemoteDeviceTabsEntry&&) noexcept = default;
CrossDeviceTabsService::RemoteDeviceTabsEntry&
CrossDeviceTabsService::RemoteDeviceTabsEntry::operator=(
    RemoteDeviceTabsEntry&&) noexcept = default;
CrossDeviceTabsService::RemoteDeviceTabsEntry::~RemoteDeviceTabsEntry() =
    default;

CrossDeviceTabsService::RemoteDeviceVisibilityEntry::
    RemoteDeviceVisibilityEntry() = default;
CrossDeviceTabsService::RemoteDeviceVisibilityEntry::
    RemoteDeviceVisibilityEntry(const RemoteDeviceVisibilityEntry&) = default;
CrossDeviceTabsService::RemoteDeviceVisibilityEntry&
CrossDeviceTabsService::RemoteDeviceVisibilityEntry::operator=(
    const RemoteDeviceVisibilityEntry&) = default;
CrossDeviceTabsService::RemoteDeviceVisibilityEntry::
    RemoteDeviceVisibilityEntry(RemoteDeviceVisibilityEntry&&) noexcept =
        default;
CrossDeviceTabsService::RemoteDeviceVisibilityEntry&
CrossDeviceTabsService::RemoteDeviceVisibilityEntry::operator=(
    RemoteDeviceVisibilityEntry&&) noexcept = default;
CrossDeviceTabsService::RemoteDeviceVisibilityEntry::
    ~RemoteDeviceVisibilityEntry() = default;

CrossDeviceTabsService::Snapshot::Snapshot() = default;
CrossDeviceTabsService::Snapshot::Snapshot(const Snapshot&) = default;
CrossDeviceTabsService::Snapshot& CrossDeviceTabsService::Snapshot::operator=(
    const Snapshot&) = default;
CrossDeviceTabsService::Snapshot::Snapshot(Snapshot&&) noexcept = default;
CrossDeviceTabsService::Snapshot& CrossDeviceTabsService::Snapshot::operator=(
    Snapshot&&) noexcept = default;
CrossDeviceTabsService::Snapshot::~Snapshot() = default;

void CrossDeviceTabsService::Observer::OnCrossDeviceTabsChanged() {}

void CrossDeviceTabsService::Observer::OnCrossDeviceTabsServiceShutdown() {}

CrossDeviceTabsService::Observer::~Observer() = default;

CrossDeviceTabsService::CrossDeviceTabsService() = default;
CrossDeviceTabsService::~CrossDeviceTabsService() = default;

}  // namespace sync_sessions
