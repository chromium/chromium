// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_DIALOG_H_
#define CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_DIALOG_H_

#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_util.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace borealis {

class BorealisMOTDDialog : public ui::WebDialogDelegate {
 public:
  // The closed callback used by the Page Handler.
  // Receives the action the user performed when closing the dialog (dismiss,
  // uninstall) as an UserMotdAction.
  using OnMotdClosedCallback = base::OnceCallback<void(UserMotdAction)>;

  static void Show(content::BrowserContext* context,
                   OnMotdClosedCallback callback);

  // Shows Borealis MOTD dialog if features::kBorealis is enabled. In common
  // cases, this is used before the Borealis splash screen.
  static void MaybeShow(content::BrowserContext* context,
                        OnMotdClosedCallback callback);

  BorealisMOTDDialog(const BorealisMOTDDialog&) = delete;
  BorealisMOTDDialog& operator=(const BorealisMOTDDialog&) = delete;
  ~BorealisMOTDDialog() override;

 private:
  BorealisMOTDDialog(content::BrowserContext* context,
                     OnMotdClosedCallback callback);
  // ui::WebDialogDelegate:
  void OnDialogClosed(const std::string& json_retval) override;

  OnMotdClosedCallback close_callback_;
};

}  // namespace borealis

#endif  // CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_DIALOG_H_
