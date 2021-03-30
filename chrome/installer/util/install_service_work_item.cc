// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/install_service_work_item.h"

#include "base/command_line.h"
#include "chrome/installer/util/install_service_work_item_impl.h"

namespace installer {

InstallServiceWorkItem::InstallServiceWorkItem(
    const std::wstring& service_name,
    const std::wstring& display_name,
    const base::CommandLine& service_cmd_line,
    const std::wstring& registry_path,
    const std::vector<GUID>& clsids,
    const std::vector<GUID>& iids)
    : impl_(std::make_unique<InstallServiceWorkItemImpl>(service_name,
                                                         display_name,
                                                         service_cmd_line,
                                                         registry_path,
                                                         clsids,
                                                         iids)) {}

InstallServiceWorkItem::~InstallServiceWorkItem() = default;

bool InstallServiceWorkItem::DoImpl() {
  return impl_->DoImpl();
}

void InstallServiceWorkItem::RollbackImpl() {
  impl_->RollbackImpl();
}

// static
bool InstallServiceWorkItem::DeleteService(const std::wstring& service_name,
                                           const std::wstring& registry_path,
                                           const std::vector<GUID>& clsids,
                                           const std::vector<GUID>& iids) {
  return InstallServiceWorkItemImpl(
             service_name, std::wstring(),
             base::CommandLine(base::CommandLine::NO_PROGRAM), registry_path,
             clsids, iids)
      .DeleteServiceImpl();
}

}  // namespace installer
