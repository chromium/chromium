// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_RECOVERY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_RECOVERY_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace crostini {
enum class CrostiniResult;

// Show the Crostini Recovery dialog when Crostini is still running after a
// Chrome crash. The user must either restart the VM, or launch a terminal.
void ShowCrostiniRecoveryView(Profile* profile,
                              CrostiniUISurface ui_surface,
                              const std::string& app_id,
                              int64_t display_id,
                              const std::vector<guest_os::LaunchArg>& args,
                              CrostiniSuccessCallback callback);

}  // namespace crostini

// Provides a warning to the user that an upgrade is required and and internet
// connection is needed.
class CrostiniRecoveryView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(CrostiniRecoveryView, views::BubbleDialogDelegateView)

 public:
  static void Show(Profile* profile,
                   const std::string& app_id,
                   int64_t display_id,
                   const std::vector<guest_os::LaunchArg>& args,
                   crostini::CrostiniSuccessCallback callback);

  // views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;

  static CrostiniRecoveryView* GetActiveViewForTesting();

 private:
  CrostiniRecoveryView(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<guest_os::LaunchArg>& args,
                       crostini::CrostiniSuccessCallback callback);
  ~CrostiniRecoveryView() override;

  void OnStopVm(crostini::CrostiniResult result);

  raw_ptr<Profile> profile_;  // Not owned.
  std::string app_id_;
  int64_t display_id_;
  const std::vector<guest_os::LaunchArg> args_;
  crostini::CrostiniSuccessCallback callback_;

  base::WeakPtrFactory<CrostiniRecoveryView> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_RECOVERY_VIEW_H_
