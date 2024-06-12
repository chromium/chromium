// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_INPUT_ROUTER_CONFIG_HELPER_H_
#define COMPONENTS_INPUT_INPUT_ROUTER_CONFIG_HELPER_H_

#include "components/input/input_router.h"

namespace input {

// Return an InputRouter configuration with parameters tailored to the current
// platform.
COMPONENT_EXPORT(INPUT)
InputRouter::Config GetInputRouterConfigForPlatform(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

}  // namespace input

#endif  // COMPONENTS_INPUT_INPUT_ROUTER_CONFIG_HELPER_H_
