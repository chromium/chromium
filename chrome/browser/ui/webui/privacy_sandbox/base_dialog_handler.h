// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_BASE_DIALOG_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_BASE_DIALOG_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager_interface.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"
#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace privacy_sandbox {

class BaseDialogUIDelegate;

class BaseDialogHandler
    : public dialog::mojom::BaseDialogPageHandler,
      public privacy_sandbox::DesktopViewManagerInterface::Observer {
 public:
  BaseDialogHandler(
      mojo::PendingReceiver<dialog::mojom::BaseDialogPageHandler> receiver,
      mojo::PendingRemote<dialog::mojom::BaseDialogPage> page,
      DesktopViewManagerInterface* view_manager,
      BaseDialogUIDelegate* delegate);

  BaseDialogHandler(const BaseDialogHandler&) = delete;
  BaseDialogHandler& operator=(const BaseDialogHandler&) = delete;

  ~BaseDialogHandler() override;

  // DesktopViewManagerInterface::Observer:
  void MaybeNavigateToNextStep(
      std::optional<privacy_sandbox::notice::mojom::PrivacySandboxNotice>
          next_id) override;

  // privacy_sandbox::dialog::mojom::BaseDialogPageHandler
  void ResizeDialog(uint32_t height) override;
  void ShowDialog() override;
  void EventOccurred(notice::mojom::PrivacySandboxNotice notice,
                     notice::mojom::PrivacySandboxNoticeEvent event) override;

 private:
  base::ScopedObservation<DesktopViewManagerInterface,
                          DesktopViewManagerInterface::Observer>
      desktop_view_manager_observation_{this};
  mojo::Receiver<dialog::mojom::BaseDialogPageHandler> receiver_;
  mojo::Remote<dialog::mojom::BaseDialogPage> page_;
  raw_ptr<BaseDialogUIDelegate> delegate_;
  raw_ptr<DesktopViewManagerInterface> view_manager_;
  bool has_resized = false;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_BASE_DIALOG_HANDLER_H_
