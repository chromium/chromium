// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_UTIL_WIN_IMPL_H_
#define CHROME_SERVICES_UTIL_WIN_UTIL_WIN_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class UtilWinImpl : public chrome::mojom::UtilWin {
 public:
  explicit UtilWinImpl(mojo::PendingReceiver<chrome::mojom::UtilWin> receiver);

  UtilWinImpl(const UtilWinImpl&) = delete;
  UtilWinImpl& operator=(const UtilWinImpl&) = delete;

  ~UtilWinImpl() override;

 private:
  // chrome::mojom::UtilWin:
  void IsPinnedToTaskbar(IsPinnedToTaskbarCallback callback) override;
  void UnpinShortcuts(const std::vector<base::FilePath>& shortcut_paths,
                      UnpinShortcutsCallback callback) override;
  void CreateOrUpdateShortcuts(
      const std::vector<base::FilePath>& shortcut_paths,
      const std::vector<base::win::ShortcutProperties>& properties,
      base::win::ShortcutOperation operation,
      CreateOrUpdateShortcutsCallback callback) override;
  void CallExecuteSelectFile(ui::SelectFileDialog::Type type,
                             uint32_t owner,
                             const std::u16string& title,
                             const base::FilePath& default_path,
                             const std::vector<ui::FileFilterSpec>& filter,
                             int32_t file_type_index,
                             const std::u16string& default_extension,
                             CallExecuteSelectFileCallback callback) override;
  void InspectModule(const base::FilePath& module_path,
                     InspectModuleCallback callback) override;
  void GetAntiVirusProducts(bool report_full_names,
                            GetAntiVirusProductsCallback callback) override;
  void GetTpmIdentifier(bool report_full_names,
                        GetTpmIdentifierCallback callback) override;

  mojo::Receiver<chrome::mojom::UtilWin> receiver_;
};

#endif  // CHROME_SERVICES_UTIL_WIN_UTIL_WIN_IMPL_H_
