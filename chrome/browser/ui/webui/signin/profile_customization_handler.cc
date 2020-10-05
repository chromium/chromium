// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

ProfileCustomizationHandler::ProfileCustomizationHandler(
    base::OnceClosure done_closure)
    : done_closure_(std::move(done_closure)) {}

ProfileCustomizationHandler::~ProfileCustomizationHandler() = default;

void ProfileCustomizationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "done", base::BindRepeating(&ProfileCustomizationHandler::HandleDone,
                                  base::Unretained(this)));
}

void ProfileCustomizationHandler::HandleDone(const base::ListValue* args) {
  if (done_closure_)
    std::move(done_closure_).Run();
}
