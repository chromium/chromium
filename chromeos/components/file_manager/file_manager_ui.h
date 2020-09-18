// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_H_
#define CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_H_

#include <memory>

#include "chromeos/components/file_manager/file_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {
namespace file_manager {

class FileManagerPageHandler;

// The WebUI controller for chrome://file-manager.
class FileManagerUI : public ui::MojoWebUIController,
                      public mojom::PageHandlerFactory {
 public:
  explicit FileManagerUI(content::WebUI* web_ui);
  ~FileManagerUI() override;

  // Disallow copy and assign.
  FileManagerUI(const FileManagerUI&) = delete;
  FileManagerUI& operator=(const FileManagerUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver);

 private:
  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::Page> pending_page,
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) override;

  mojo::Receiver<mojom::PageHandlerFactory> page_factory_receiver_{this};
  std::unique_ptr<FileManagerPageHandler> page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace file_manager
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_UI_H_
