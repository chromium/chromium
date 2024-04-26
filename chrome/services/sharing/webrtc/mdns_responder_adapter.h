// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_MDNS_RESPONDER_ADAPTER_H_
#define CHROME_SERVICES_SHARING_WEBRTC_MDNS_RESPONDER_ADAPTER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "third_party/webrtc/rtc_base/mdns_responder_interface.h"

namespace rtc {
class IPAddress;
}  // namespace rtc

namespace sharing {

// The MdnsResponderAdapter implements the WebRTC mDNS responder interface via
// the MdnsResponder service in Chromium, and is used to register and resolve
// mDNS hostnames to conceal local IP addresses.
// TODO(crbug.com/40115622): reuse code from blink instead.
class MdnsResponderAdapter : public webrtc::MdnsResponderInterface {
 public:
  explicit MdnsResponderAdapter(
      mojo::Remote<network::mojom::MdnsResponder> mdns_responder);
  MdnsResponderAdapter(const MdnsResponderAdapter&) = delete;
  MdnsResponderAdapter& operator=(const MdnsResponderAdapter&) = delete;
  ~MdnsResponderAdapter() override;

  // webrtc::MdnsResponderInterface:
  void CreateNameForAddress(const rtc::IPAddress& addr,
                            NameCreatedCallback callback) override;
  void RemoveNameForAddress(const rtc::IPAddress& addr,
                            NameRemovedCallback callback) override;

 private:
  mojo::Remote<network::mojom::MdnsResponder> mdns_responder_;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_MDNS_RESPONDER_ADAPTER_H_
