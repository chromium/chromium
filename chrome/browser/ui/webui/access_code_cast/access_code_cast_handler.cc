// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"

AccessCodeCastHandler::AccessCodeCastHandler(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
    mojo::PendingRemote<access_code_cast::mojom::Page> page)
    : page_(std::move(page)), receiver_(this, std::move(page_handler)) {}

AccessCodeCastHandler::~AccessCodeCastHandler() = default;

void AccessCodeCastHandler::AddSink(
    const std::string& access_code,
    access_code_cast::mojom::CastDiscoveryMethod discovery_method,
    AddSinkCallback callback) {
  // TODO (b/204571687): Complete communication with the discovery server once
  // implementation has finished on the discovery server interface
}

void AccessCodeCastHandler::CastToSink(CastToSinkCallback callback) {
  // TODO (b/204572061): Complete casting implementation
}
