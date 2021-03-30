// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/feedback_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "extensions/common/api/feedback_private.h"

FeedbackHandler::FeedbackHandler(const FeedbackDialog* dialog)
    : dialog_(dialog) {}

FeedbackHandler::~FeedbackHandler() = default;

void FeedbackHandler::HandleShowDialog(const base::ListValue* args) {
  dialog_->Show();
}

void FeedbackHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showDialog", base::BindRepeating(&FeedbackHandler::HandleShowDialog,
                                        base::Unretained(this)));
}
