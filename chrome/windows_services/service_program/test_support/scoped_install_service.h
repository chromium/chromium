// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_INSTALL_SERVICE_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_INSTALL_SERVICE_H_

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/win/windows_types.h"

namespace installer {
class InstallServiceWorkItem;
}

class ScopedInstallService {
 public:
  ScopedInstallService(std::wstring_view service_name,
                       std::wstring_view display_name,
                       std::wstring_view description,
                       base::FilePath::StringPieceType exe_name,
                       std::string_view testing_switch,
                       const CLSID& clsid,
                       const IID& iid);
  ScopedInstallService(const ScopedInstallService&) = delete;
  ScopedInstallService& operator=(const ScopedInstallService&) = delete;
  ~ScopedInstallService();

  bool is_valid() const { return bool(work_item_); }

 private:
  std::unique_ptr<installer::InstallServiceWorkItem> work_item_;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SCOPED_INSTALL_SERVICE_H_
