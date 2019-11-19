// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_UTIL_WIN_IMPL_H_
#define CHROME_SERVICES_UTIL_WIN_UTIL_WIN_IMPL_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class UtilWinImpl : public chrome::mojom::UtilWin {
 public:
  explicit UtilWinImpl(mojo::PendingReceiver<chrome::mojom::UtilWin> receiver);
  ~UtilWinImpl() override;

 private:
  // chrome::mojom::UtilWin:
  void IsPinnedToTaskbar(IsPinnedToTaskbarCallback callback) override;
  void CallExecuteSelectFile(ui::SelectFileDialog::Type type,
                             uint32_t owner,
                             const base::string16& title,
                             const base::FilePath& default_path,
                             const std::vector<ui::FileFilterSpec>& filter,
                             int32_t file_type_index,
                             const base::string16& default_extension,
                             CallExecuteSelectFileCallback callback) override;
  void InspectModule(const base::FilePath& module_path,
                     InspectModuleCallback callback) override;
  void GetAntiVirusProducts(bool report_full_names,
                            GetAntiVirusProductsCallback callback) override;

  mojo::Receiver<chrome::mojom::UtilWin> receiver_;

  DISALLOW_COPY_AND_ASSIGN(UtilWinImpl);
};

#endif  // CHROME_SERVICES_UTIL_WIN_UTIL_WIN_IMPL_H_
