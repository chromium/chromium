// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/password_protection/request_canceler.h"

namespace safe_browsing {

CancelableRequest::~CancelableRequest() = default;

RequestCanceler::RequestCanceler(base::WeakPtr<CancelableRequest> request)
    : request_(request) {}

RequestCanceler::~RequestCanceler() = default;

}  // namespace safe_browsing