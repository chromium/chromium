// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/mdns_responder_adapter.h"

#include <string>

#include "base/bind.h"
#include "jingle/glue/utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_address.h"
#include "third_party/webrtc/rtc_base/ip_address.h"

namespace sharing {

namespace {

void OnNameCreatedForAddress(
    webrtc::MdnsResponderInterface::NameCreatedCallback callback,
    const rtc::IPAddress& addr,
    const std::string& name,
    bool announcement_scheduled) {
  // We currently ignore whether there is an announcement sent for the name.
  callback(addr, name);
}

void OnNameRemovedForAddress(
    webrtc::MdnsResponderInterface::NameRemovedCallback callback,
    bool removed,
    bool goodbye_scheduled) {
  // We currently ignore whether there is a goodbye sent for the name.
  callback(removed);
}

}  // namespace

MdnsResponderAdapter::MdnsResponderAdapter(
    const mojo::SharedRemote<network::mojom::MdnsResponder>& mdns_responder)
    : mdns_responder_(mdns_responder) {
  DCHECK(mdns_responder_.is_bound());
}

MdnsResponderAdapter::~MdnsResponderAdapter() = default;

void MdnsResponderAdapter::CreateNameForAddress(const rtc::IPAddress& addr,
                                                NameCreatedCallback callback) {
  mdns_responder_->CreateNameForAddress(
      jingle_glue::RtcIPAddressToNetIPAddress(addr),
      base::BindOnce(&OnNameCreatedForAddress, callback, addr));
}

void MdnsResponderAdapter::RemoveNameForAddress(const rtc::IPAddress& addr,
                                                NameRemovedCallback callback) {
  mdns_responder_->RemoveNameForAddress(
      jingle_glue::RtcIPAddressToNetIPAddress(addr),
      base::BindOnce(&OnNameRemovedForAddress, callback));
}

}  // namespace sharing
