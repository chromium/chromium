// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/enterprise_casting/enterprise_casting_handler.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"

EnterpriseCastingHandler::EnterpriseCastingHandler(
    mojo::PendingReceiver<enterprise_casting::mojom::PageHandler> page_handler,
    mojo::PendingRemote<enterprise_casting::mojom::Page> page)
    : page_(std::move(page)), receiver_(this, std::move(page_handler)) {}

EnterpriseCastingHandler::~EnterpriseCastingHandler() = default;

void EnterpriseCastingHandler::AddSink(
    const std::string& access_code,
    enterprise_casting::mojom::CastDiscoveryMethod discovery_method,
    AddSinkCallback callback) {
  // TODO (b/204571687): Complete communication with the discovery server once
  // implementation has finished on the discovery server interface
}

void EnterpriseCastingHandler::CastToSink(CastToSinkCallback callback) {
  // TODO (b/204572061): Complete casting implementation
}
