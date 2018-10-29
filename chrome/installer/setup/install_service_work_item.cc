// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_service_work_item.h"

#include "base/command_line.h"
#include "chrome/installer/setup/install_service_work_item_impl.h"

namespace installer {

InstallServiceWorkItem::InstallServiceWorkItem(
    const base::string16& service_name,
    const base::string16& display_name,
    const base::CommandLine& service_cmd_line)
    : impl_(std::make_unique<InstallServiceWorkItemImpl>(service_name,
                                                         display_name,
                                                         service_cmd_line)) {}

InstallServiceWorkItem::~InstallServiceWorkItem() = default;

bool InstallServiceWorkItem::DoImpl() {
  return impl_->DoImpl();
}

void InstallServiceWorkItem::RollbackImpl() {
  impl_->RollbackImpl();
}

// static
bool InstallServiceWorkItem::DeleteService(const base::string16& service_name) {
  return InstallServiceWorkItemImpl(
             service_name, base::string16(),
             base::CommandLine(base::CommandLine::NO_PROGRAM))
      .DeleteServiceImpl();
}

}  // namespace installer
