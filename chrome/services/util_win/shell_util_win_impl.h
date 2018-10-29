// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_SHELL_UTIL_WIN_IMPL_H_
#define CHROME_SERVICES_UTIL_WIN_SHELL_UTIL_WIN_IMPL_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/services/util_win/public/mojom/shell_util_win.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

class ShellUtilWinImpl : public chrome::mojom::ShellUtilWin {
 public:
  explicit ShellUtilWinImpl(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~ShellUtilWinImpl() override;

 private:
  // chrome::mojom::ShellUtilWin:
  void IsPinnedToTaskbar(IsPinnedToTaskbarCallback callback) override;
  void CallExecuteSelectFile(ui::SelectFileDialog::Type type,
                             uint32_t owner,
                             const base::string16& title,
                             const base::FilePath& default_path,
                             const std::vector<ui::FileFilterSpec>& filter,
                             int32_t file_type_index,
                             const base::string16& default_extension,
                             CallExecuteSelectFileCallback callback) override;

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  DISALLOW_COPY_AND_ASSIGN(ShellUtilWinImpl);
};

#endif  // CHROME_SERVICES_UTIL_WIN_SHELL_UTIL_WIN_IMPL_H_
