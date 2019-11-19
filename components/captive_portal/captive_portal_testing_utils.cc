// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/captive_portal_testing_utils.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace {

scoped_refptr<net::HttpResponseHeaders> CreateResponseHeaders(
    base::StringPiece response_headers) {
  std::string raw_headers = net::HttpUtil::AssembleRawHeaders(response_headers);
  return base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
}

}  // namespace

namespace captive_portal {

CaptivePortalDetectorTestBase::CaptivePortalDetectorTestBase()
    : detector_(nullptr) {}

CaptivePortalDetectorTestBase::~CaptivePortalDetectorTestBase() {
}

void CaptivePortalDetectorTestBase::SetTime(const base::Time& time) {
  detector()->set_time_for_testing(time);
}

void CaptivePortalDetectorTestBase::AdvanceTime(const base::TimeDelta& delta) {
  detector()->advance_time_for_testing(delta);
}

bool CaptivePortalDetectorTestBase::FetchingURL() {
  return detector()->FetchingURL();
}

void CaptivePortalDetectorTestBase::CompleteURLFetch(
    int net_error,
    int status_code,
    const char* response_headers) {
  scoped_refptr<net::HttpResponseHeaders> headers;
  if (response_headers)
    headers = CreateResponseHeaders(response_headers);
  detector()->OnSimpleLoaderCompleteInternal(net_error, status_code, GURL(),
                                             headers.get());
}

}  // namespace captive_portal
