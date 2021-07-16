// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/desk_template_read_handler.h"

#include "base/no_destructor.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"

namespace full_restore {

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
  if (!delegate_ || delegate_->IsFullRestoreRunning())
    return nullptr;
  return delegate_->GetWindowInfo(restore_window_id);
}

int32_t DeskTemplateReadHandler::FetchRestoreWindowId(
    const std::string& app_id) {
  if (!delegate_ || delegate_->IsFullRestoreRunning())
    return 0;
  return delegate_->FetchRestoreWindowId(app_id);
}

}  // namespace full_restore
