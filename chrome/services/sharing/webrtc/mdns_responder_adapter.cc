// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/mdns_responder_adapter.h"

#include <string>

#include "base/functional/bind.h"
#include "components/webrtc/net_address_utils.h"
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
    mojo::Remote<network::mojom::MdnsResponder> mdns_responder)
    : mdns_responder_(std::move(mdns_responder)) {
  DCHECK(mdns_responder_.is_bound());
}

MdnsResponderAdapter::~MdnsResponderAdapter() = default;

void MdnsResponderAdapter::CreateNameForAddress(const rtc::IPAddress& addr,
                                                NameCreatedCallback callback) {
  if (!mdns_responder_ || !mdns_responder_.is_connected()) {
    LOG(ERROR) << "MdnsResponderAdapter::" << __func__ << ": mDNS responder"
               << " no longer available over mojo, returning empty name.";
    // If the responder is no longer available we trigger the callback now with
    // no name since this the only way we can signal an error.
    callback(addr, std::string());
    return;
  }

  mdns_responder_->CreateNameForAddress(
      webrtc::RtcIPAddressToNetIPAddress(addr),
      base::BindOnce(&OnNameCreatedForAddress, callback, addr));
}

void MdnsResponderAdapter::RemoveNameForAddress(const rtc::IPAddress& addr,
                                                NameRemovedCallback callback) {
  if (!mdns_responder_ || !mdns_responder_.is_connected()) {
    LOG(ERROR) << "MdnsResponderAdapter::" << __func__ << ": mDNS responder"
               << " no longer available over mojo, returning false.";
    // If the responder is no longer available we trigger the callback now.
    callback(false);
    return;
  }

  mdns_responder_->RemoveNameForAddress(
      webrtc::RtcIPAddressToNetIPAddress(addr),
      base::BindOnce(&OnNameRemovedForAddress, callback));
}

}  // namespace sharing
