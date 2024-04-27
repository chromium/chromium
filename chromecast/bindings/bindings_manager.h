// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_H_

#include "components/cast/api_bindings/manager.h"

namespace chromecast {
namespace bindings {

// TODO(crbug.com/40139651): Remove this alias when all callers are migrated to
// use cast_api_bindings::Manager directly.
class BindingsManager : public cast_api_bindings::Manager {};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_H_
