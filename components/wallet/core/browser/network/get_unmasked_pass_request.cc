// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/get_unmasked_pass_request.h"

#include "base/notimplemented.h"

namespace wallet {

GetUnmaskedPassRequest::GetUnmaskedPassRequest(
    std::string pass_id,
    WalletHttpClient::GetUnmaskedPassCallback callback)
    : pass_id_(pass_id), callback_(std::move(callback)) {}

GetUnmaskedPassRequest::~GetUnmaskedPassRequest() = default;

std::string GetUnmaskedPassRequest::GetRequestUrlPath() const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string GetUnmaskedPassRequest::GetRequestContent() const {
  NOTIMPLEMENTED();
  return std::string();
}

void GetUnmaskedPassRequest::OnResponse(
    WalletHttpClient::HttpResponse http_response) && {
  NOTIMPLEMENTED();
}

}  // namespace wallet
