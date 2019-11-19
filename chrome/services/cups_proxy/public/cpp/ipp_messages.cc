// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/ipp_messages.h"

#include "base/strings/string_piece.h"

namespace cups_proxy {

// Defaults for IppRequest.
IppRequest::IppRequest() : ipp(printing::WrapIpp(nullptr)) {}
IppRequest::IppRequest(IppRequest&& other) = default;
IppRequest::~IppRequest() = default;

// Defaults for IppResponse.
IppResponse::IppResponse() : ipp(printing::WrapIpp(nullptr)) {}
IppResponse::IppResponse(IppResponse&& other) = default;
IppResponse::~IppResponse() = default;

}  // namespace cups_proxy
