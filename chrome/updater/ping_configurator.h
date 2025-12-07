// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PING_CONFIGURATOR_H_
#define CHROME_UPDATER_PING_CONFIGURATOR_H_

#include "base/memory/scoped_refptr.h"
#include "components/update_client/configurator.h"

namespace updater {

// This is a lightweight `Configurator` that has very few dependencies. The
// implementation has `NOTREACHED` for many methods, and is solely expected to
// be used for standalone pings.
scoped_refptr<update_client::Configurator> CreatePingConfigurator();

}  // namespace updater

#endif  // CHROME_UPDATER_PING_CONFIGURATOR_H_
