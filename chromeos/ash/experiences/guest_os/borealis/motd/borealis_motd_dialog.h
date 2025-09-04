// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_DIALOG_H_
#define CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_DIALOG_H_

#include "ash/constants/url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace borealis {

// Show Borealis MOTD dialog if features::kBorealis is enabled before the
// Borealis splash screen.
void MaybeShowBorealisMOTDDialog(base::OnceCallback<void()> cb,
                                 content::BrowserContext* context);

// Forward declaration so that config definition can come before controller.
class BorealisMOTDUI;

class BorealisMOTDUIConfig
    : public content::DefaultWebUIConfig<BorealisMOTDUI> {
 public:
  BorealisMOTDUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIBorealisMOTDHost) {}
};

// The WebUI for chrome://borealis-motd
class BorealisMOTDUI : public content::WebUIController {
 public:
  explicit BorealisMOTDUI(content::WebUI* web_ui);
  ~BorealisMOTDUI() override;
};

class BorealisMOTDDialog : public ui::WebDialogDelegate {
 public:
  static void Show(base::OnceCallback<void()> cb,
                   content::BrowserContext* context);
  BorealisMOTDDialog(const BorealisMOTDDialog&) = delete;
  BorealisMOTDDialog& operator=(const BorealisMOTDDialog&) = delete;
  ~BorealisMOTDDialog() override;

 private:
  BorealisMOTDDialog(base::OnceCallback<void()>,
                     content::BrowserContext* context);
  // ui::WebDialogDelegate:
  void OnDialogClosed(const std::string& json_retval) override;
  void OnLoadingStateChanged(content::WebContents* source) override;

  base::OnceCallback<void()> close_callback_;
};

}  // namespace borealis

#endif  // CHROMEOS_ASH_EXPERIENCES_GUEST_OS_BOREALIS_MOTD_BOREALIS_MOTD_DIALOG_H_
