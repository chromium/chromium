// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This module is responsible for installing a service, given a |service_name|,
// |display_name|, and |service_cmd_line|. If the service already exists, a
// light-weight upgrade of the service will be performed, to reduce the chances
// of anti-virus flagging issues with deleting/installing a new service.
// In the event that the upgrade fails, this module will install a new service
// and mark the original service for deletion.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_SERVICE_WORK_ITEM_H_
#define CHROME_INSTALLER_SETUP_INSTALL_SERVICE_WORK_ITEM_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/installer/util/work_item.h"

namespace base {
class CommandLine;
}  // namespace base

namespace installer {

class InstallServiceWorkItemImpl;

// A generic WorkItem subclass that installs a Windows Service for Chrome.
class InstallServiceWorkItem : public WorkItem {
 public:
  // |service_name| is the name given to the service. In the case of a conflict
  // when upgrading the service, this will be the prefix for a versioned name
  // given to the service.
  // An example |service_name| could be "elevationservice".
  //
  // |display_name| is the human-readable name that is visible in the Service
  // control panel. For example, "Chrome Elevation Service".
  //
  // |service_cmd_line| is the command line with which the service is invoked by
  // the SCM. For example,
  // "C:\Program Files (x86)\Google\Chrome\ElevationService.exe" /svc
  InstallServiceWorkItem(const base::string16& service_name,
                         const base::string16& display_name,
                         const base::CommandLine& service_cmd_line);

  ~InstallServiceWorkItem() override;

  static bool DeleteService(const base::string16& service_name);

 private:
  friend class InstallServiceWorkItemTest;

  // Overrides of WorkItem.
  bool DoImpl() override;
  void RollbackImpl() override;

  std::unique_ptr<InstallServiceWorkItemImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(InstallServiceWorkItem);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_SERVICE_WORK_ITEM_H_
