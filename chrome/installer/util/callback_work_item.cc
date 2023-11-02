// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/callback_work_item.h"

CallbackWorkItem::CallbackWorkItem(
    base::OnceCallback<bool(const CallbackWorkItem&)> do_action,
    base::OnceCallback<void(const CallbackWorkItem&)> rollback_action)
    : do_action_(std::move(do_action)),
      rollback_action_(std::move(rollback_action)) {}

CallbackWorkItem::~CallbackWorkItem() = default;

bool CallbackWorkItem::DoImpl() {
  return std::move(do_action_).Run(*this);
}

void CallbackWorkItem::RollbackImpl() {
  std::move(rollback_action_).Run(*this);
}
