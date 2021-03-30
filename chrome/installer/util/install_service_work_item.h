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

#ifndef CHROME_INSTALLER_UTIL_INSTALL_SERVICE_WORK_ITEM_H_
#define CHROME_INSTALLER_UTIL_INSTALL_SERVICE_WORK_ITEM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
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
  //
  // NOTE: |registry_path| is mapped to the 32-bit view of the registry for
  // legacy reasons. |registry_path| is the path in HKEY_LOCAL_MACHINE under
  // which the service persists information, for instance if the service has to
  // persist a versioned service name. An example |registry_path| is
  // "Software\ProductFoo".
  //
  // If COM CLSID/AppId registration is required, |clsids| should contain the
  // CLSIDs and AppIds to register. If COM Interface/Typelib registration is
  // required, |iids| should contain the Interfaces and Typelibs to register.
  InstallServiceWorkItem(const std::wstring& service_name,
                         const std::wstring& display_name,
                         const base::CommandLine& service_cmd_line,
                         const std::wstring& registry_path,
                         const std::vector<GUID>& clsids,
                         const std::vector<GUID>& iids);

  ~InstallServiceWorkItem() override;

  static bool DeleteService(const std::wstring& service_name,
                            const std::wstring& registry_path,
                            const std::vector<GUID>& clsids,
                            const std::vector<GUID>& iids);

 private:
  friend class InstallServiceWorkItemTest;

  // Overrides of WorkItem.
  bool DoImpl() override;
  void RollbackImpl() override;

  std::unique_ptr<InstallServiceWorkItemImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(InstallServiceWorkItem);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INSTALL_SERVICE_WORK_ITEM_H_
