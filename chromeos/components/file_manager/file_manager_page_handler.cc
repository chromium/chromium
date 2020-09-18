// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/file_manager/file_manager_page_handler.h"

#include "base/bind.h"

namespace chromeos {
namespace file_manager {

FileManagerPageHandler::FileManagerPageHandler(
    mojo::PendingReceiver<mojom::PageHandler> pending_receiver,
    mojo::PendingRemote<mojom::Page> pending_page)
    : receiver_{this, std::move(pending_receiver)},
      page_{std::move(pending_page)} {}

FileManagerPageHandler::~FileManagerPageHandler() = default;

void FileManagerPageHandler::GetFoo(GetFooCallback callback) {
  std::move(callback).Run(foo_);
}

void FileManagerPageHandler::SetFoo(const std::string& foo) {
  foo_ = foo;
}

void FileManagerPageHandler::DoABarrelRoll() {
  barrel_roll_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(1),
      base::BindOnce(&FileManagerPageHandler::OnBarrelRollDone,
                     base::Unretained(this)));
}

void FileManagerPageHandler::OnBarrelRollDone() {
  page_->GetBar(foo_, base::BindOnce(&FileManagerPageHandler::OnBarReceived,
                                     base::Unretained(this)));
}

void FileManagerPageHandler::OnBarReceived(const std::string& bar) {
  page_->OnSomethingHappened(foo_, bar);
}

}  // namespace file_manager
}  // namespace chromeos
