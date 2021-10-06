// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/desk_template_read_handler.h"

#include "base/no_destructor.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"

namespace app_restore {

// static
DeskTemplateReadHandler* DeskTemplateReadHandler::GetInstance() {
  static base::NoDestructor<DeskTemplateReadHandler> desk_template_read_handler;
  return desk_template_read_handler.get();
}

DeskTemplateReadHandler::DeskTemplateReadHandler() = default;

DeskTemplateReadHandler::~DeskTemplateReadHandler() = default;

void DeskTemplateReadHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

std::unique_ptr<WindowInfo> DeskTemplateReadHandler::GetWindowInfo(
    int restore_window_id) {
  return delegate_ ? delegate_->GetWindowInfo(restore_window_id) : nullptr;
}

int32_t DeskTemplateReadHandler::FetchRestoreWindowId(
    const std::string& app_id) {
  return delegate_ ? delegate_->FetchRestoreWindowId(app_id) : 0;
}

}  // namespace app_restore
