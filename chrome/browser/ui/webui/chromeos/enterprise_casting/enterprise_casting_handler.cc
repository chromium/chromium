// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/enterprise_casting/enterprise_casting_handler.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"

namespace chromeos {

EnterpriseCastingHandler::EnterpriseCastingHandler(
    mojo::PendingReceiver<enterprise_casting::mojom::PageHandler> page_handler,
    mojo::PendingRemote<enterprise_casting::mojom::Page> page)
    : page_(std::move(page)), receiver_(this, std::move(page_handler)) {}

EnterpriseCastingHandler::~EnterpriseCastingHandler() = default;

void EnterpriseCastingHandler::UpdatePin() {
  page_->SetPin(base::NumberToString(base::RandInt(0, 9999)));
}

}  // namespace chromeos
