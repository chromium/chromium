// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/secure_channel_disconnector_impl.h"

#include "base/memory/ptr_util.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {

namespace secure_channel {

// static
SecureChannelDisconnectorImpl::Factory*
    SecureChannelDisconnectorImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<SecureChannelDisconnector>
SecureChannelDisconnectorImpl::Factory::Create() {
  if (test_factory_)
    return test_factory_->CreateInstance();

  return base::WrapUnique(new SecureChannelDisconnectorImpl());
}

// static
void SecureChannelDisconnectorImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

SecureChannelDisconnectorImpl::Factory::~Factory() = default;

SecureChannelDisconnectorImpl::SecureChannelDisconnectorImpl() = default;

SecureChannelDisconnectorImpl::~SecureChannelDisconnectorImpl() = default;

void SecureChannelDisconnectorImpl::DisconnectSecureChannel(
    std::unique_ptr<SecureChannel> channel_to_disconnect) {
  // If |channel_to_disconnect| was already DISCONNECTING, this function is a
  // no-op. If |channel_to_disconnecting| was CONNECTING, this function
  // immediately causes the channel to switch to DISCONNECTED. Both of these
  // cases trigger an early return below.
  channel_to_disconnect->Disconnect();
  if (channel_to_disconnect->status() == SecureChannel::Status::DISCONNECTED) {
    return;
  }

  // If no early return occurred, |channel_to_disconnect| is now DISCONNECTING.
  DCHECK_EQ(SecureChannel::Status::DISCONNECTING,
            channel_to_disconnect->status());

  // Observe |channel_to_disconnect| so that we can be alerted when it does
  // eventually transition to DISCONNECTED.
  channel_to_disconnect->AddObserver(this);
  disconnecting_channels_.insert(std::move(channel_to_disconnect));
}

void SecureChannelDisconnectorImpl::OnSecureChannelStatusChanged(
    SecureChannel* secure_channel,
    const SecureChannel::Status& old_status,
    const SecureChannel::Status& new_status) {
  if (new_status != SecureChannel::Status::DISCONNECTED)
    return;

  for (auto it = disconnecting_channels_.begin();
       it != disconnecting_channels_.end(); ++it) {
    if (secure_channel == it->get()) {
      (*it)->RemoveObserver(this);
      disconnecting_channels_.erase(it);
      return;
    }
  }

  PA_LOG(ERROR) << "SecureChannelDisconnectorImpl::"
                << "OnSecureChannelStatusChanged(): Channel was disconnected, "
                << "but it was not being tracked.";
  NOTREACHED();
}

}  // namespace secure_channel

}  // namespace chromeos
