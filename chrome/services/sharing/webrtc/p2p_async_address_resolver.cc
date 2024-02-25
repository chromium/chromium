// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/p2p_async_address_resolver.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "components/webrtc/net_address_utils.h"

namespace sharing {

P2PAsyncAddressResolver::P2PAsyncAddressResolver(
    const mojo::SharedRemote<network::mojom::P2PSocketManager>& socket_manager)
    : socket_manager_(socket_manager), state_(STATE_CREATED) {
  DCHECK(socket_manager_.is_bound());
}

P2PAsyncAddressResolver::~P2PAsyncAddressResolver() {
  DCHECK(state_ == STATE_CREATED || state_ == STATE_FINISHED);
}

void P2PAsyncAddressResolver::Start(const rtc::SocketAddress& host_name,
                                    std::optional<int> address_family,
                                    DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(STATE_CREATED, state_);

  state_ = STATE_SENT;
  done_callback_ = std::move(done_callback);
  if (address_family.has_value()) {
    socket_manager_->GetHostAddressWithFamily(
        host_name.hostname(), address_family.value(), /*enable_mdns=*/true,
        base::BindOnce(&P2PAsyncAddressResolver::OnResponse,
                       base::Unretained(this)));
  } else {
    socket_manager_->GetHostAddress(
        host_name.hostname(), /*enable_mdns=*/true,
        base::BindOnce(&P2PAsyncAddressResolver::OnResponse,
                       base::Unretained(this)));
  }
}

void P2PAsyncAddressResolver::Cancel() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ != STATE_FINISHED)
    state_ = STATE_FINISHED;

  done_callback_.Reset();
}

void P2PAsyncAddressResolver::OnResponse(
    const std::vector<net::IPAddress>& addresses) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ == STATE_SENT) {
    state_ = STATE_FINISHED;
    std::move(done_callback_).Run(addresses);
  }
}

}  // namespace sharing
