// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keep_alive_registry/scoped_keep_alive.h"

#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"

ScopedKeepAlive::ScopedKeepAlive(KeepAliveOrigin origin,
                                 KeepAliveRestartOption restart)
    : origin_(origin), restart_(restart) {
  KeepAliveRegistry::GetInstance()->Register(origin_, restart_);
}

ScopedKeepAlive::~ScopedKeepAlive() {
  KeepAliveRegistry::GetInstance()->Unregister(origin_, restart_);
}
