// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_PAGE_HANDLER_H_
#define CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_PAGE_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chromeos/components/file_manager/file_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace file_manager {

// Class backing the page's functionality.
class FileManagerPageHandler : public mojom::PageHandler {
 public:
  FileManagerPageHandler(
      mojo::PendingReceiver<mojom::PageHandler> pending_receiver,
      mojo::PendingRemote<mojom::Page> pending_page);
  ~FileManagerPageHandler() override;

 private:
  // mojom::PageHandler:
  void GetFoo(GetFooCallback callback) override;
  void SetFoo(const std::string& foo) override;
  void DoABarrelRoll() override;

  void OnBarrelRollDone();
  void OnBarReceived(const std::string& bar);

  mojo::Receiver<mojom::PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;

  std::string foo_;
  base::OneShotTimer barrel_roll_timer_;

  DISALLOW_COPY_AND_ASSIGN(FileManagerPageHandler);
};

}  // namespace file_manager
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_FILE_MANAGER_FILE_MANAGER_PAGE_HANDLER_H_
