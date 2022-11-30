// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_resource.h"

namespace chromecast {

void CastResource::SetCastResourceClient(Client* client) {
  client_ = client;
}

void CastResource::RegisterWithClient() {
  if (client_)
    client_->RegisterCastResource(this);
}

void CastResource::NotifyResourceReleased(Resource remain) {
  if (client_)
    client_->OnResourceReleased(this, remain);
}

}  // namespace chromecast
