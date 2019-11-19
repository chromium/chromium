// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_udp_transport.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/renderer/media/cast_session.h"

CastUdpTransport::CastUdpTransport(const scoped_refptr<CastSession>& session)
    : cast_session_(session), options_(new base::DictionaryValue) {}

CastUdpTransport::~CastUdpTransport() {
}

void CastUdpTransport::SetDestination(
    const net::IPEndPoint& remote_address,
    const CastSessionDelegate::ErrorCallback& error_callback) {
  DVLOG(1) << "CastUdpTransport::SetDestination = "
           << remote_address.ToString();
  remote_address_ = remote_address;
  cast_session_->StartUDP(
      remote_address, base::WrapUnique(options_->DeepCopy()), error_callback);
}

void CastUdpTransport::SetOptions(
    std::unique_ptr<base::DictionaryValue> options) {
  options_ = std::move(options);
}
