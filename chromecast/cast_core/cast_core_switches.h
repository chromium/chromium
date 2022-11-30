// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_CAST_CORE_SWITCHES_H_
#define CHROMECAST_CAST_CORE_CAST_CORE_SWITCHES_H_

namespace cast {
namespace core {

// Specifies the Cast Core runtime ID, --cast-core-runtime-id=<runtime_id>.
constexpr char kCastCoreRuntimeIdSwitch[] = "cast-core-runtime-id";

// Specifies the Cast Core runtime gRPC endpoint,
// --runtime-service-path=<endpoint>.
constexpr char kRuntimeServicePathSwitch[] = "runtime-service-path";

}  // namespace core
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_CAST_CORE_SWITCHES_H_
