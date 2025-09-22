// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/install_service_work_item.h"

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "chrome/installer/util/install_service_work_item_impl.h"

namespace installer {

InstallServiceWorkItem::InstallServiceWorkItem(
    const std::wstring& service_name,
    const std::wstring& display_name,
    const std::wstring& description,
    uint32_t start_type,
    const base::CommandLine& service_cmd_line,
    const base::CommandLine& com_service_cmd_line_args,
    const std::wstring& registry_path,
    const std::vector<GUID>& clsids,
    const std::vector<GUID>& iids)
    : impl_(std::make_unique<InstallServiceWorkItemImpl>(
          service_name,
          display_name,
          description,
          start_type,
          service_cmd_line,
          com_service_cmd_line_args,
          registry_path,
          clsids,
          iids)) {}

InstallServiceWorkItem::~InstallServiceWorkItem() = default;

bool InstallServiceWorkItem::DoImpl() {
  if (impl_->DoImpl()) {
    return true;
  }
  // TODO(crbug.com/443552436): The overall installation succeeds if updating
  // the elevation service fails, yet saved passwords will be unavailable until
  // the next update repairs it. Send up a dump in case of failure so that we
  // can see what's happening in the process and the most recent errors added to
  // the installer's log file.
  base::debug::DumpWithoutCrashing();
  return false;
}

void InstallServiceWorkItem::RollbackImpl() {
  impl_->RollbackImpl();
}

// static
bool InstallServiceWorkItem::DeleteService(const std::wstring& service_name,
                                           const std::wstring& registry_path,
                                           const std::vector<GUID>& clsids,
                                           const std::vector<GUID>& iids) {
  // The `display_name`, `description`, `start_type`, `service_cmd_line`, and
  // `com_service_cmd_line_args` are ignored by `InstallServiceWorkItemImpl` for
  // `DeleteServiceImpl`.
  return InstallServiceWorkItemImpl(
             service_name, /*display_name=*/{}, /*description=*/{},
             SERVICE_DISABLED, base::CommandLine(base::CommandLine::NO_PROGRAM),
             base::CommandLine(base::CommandLine::NO_PROGRAM), registry_path,
             clsids, iids)
      .DeleteServiceImpl();
}

// static
bool InstallServiceWorkItem::IsComServiceInstalled(const GUID& clsid) {
  return InstallServiceWorkItemImpl::IsComServiceInstalled(clsid);
}

// static
std::wstring InstallServiceWorkItem::GetCurrentServiceName(
    base::wcstring_view service_name,
    base::wcstring_view registry_path) {
  return InstallServiceWorkItemImpl::GetCurrentServiceName(service_name,
                                                           registry_path);
}

}  // namespace installer
