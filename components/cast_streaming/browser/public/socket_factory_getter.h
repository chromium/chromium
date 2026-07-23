// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_SOCKET_FACTORY_GETTER_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_SOCKET_FACTORY_GETTER_H_

#include "components/openscreen_platform/socket_factory.h"

namespace cast_streaming::SocketFactoryGetter {

// Sets the SocketFactoryGetter for embedders.
// This must be called before any call to
// ReceiverSession::SetCastStreamingReceiver() and must only be called once.
void Set(openscreen_platform::SocketFactoryGetter::Callback getter);

// Clears the SocketFactoryGetter set above, if it has been set.
void Clear();

// Checks whether the above factory getter has been set.
bool IsSet();

}  // namespace cast_streaming::SocketFactoryGetter

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_PUBLIC_SOCKET_FACTORY_GETTER_H_
