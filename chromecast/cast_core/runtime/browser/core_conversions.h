// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CORE_CONVERSIONS_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CORE_CONVERSIONS_H_

#include "components/cast_receiver/browser/public/application_config.h"

namespace cast::common {
class ApplicationConfig;
}  // namespace cast::common

namespace chromecast {

cast_receiver::ApplicationConfig ToReceiverConfig(
    const cast::common::ApplicationConfig& core_config);

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CORE_CONVERSIONS_H_
