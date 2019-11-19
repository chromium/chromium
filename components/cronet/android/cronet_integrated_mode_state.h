// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_INTEGRATED_MODE_STATE_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_INTEGRATED_MODE_STATE_H_

#include "base/task/thread_pool/thread_pool_instance.h"

namespace cronet {

/**
 * Set a shared network task runner into Cronet in integrated mode. All the
 * Cronet network tasks would be running in this task runner. This method should
 * be invoked in native side before creating Cronet instance.
 */
void SetIntegratedModeNetworkTaskRunner(
    base::SingleThreadTaskRunner* network_task_runner);

/**
 * Get the task runner for Cronet integrated mode. It would be invoked in the
 * initialization of CronetURLRequestContext. This method must be invoked after
 * SetIntegratedModeNetworkTaskRunner.
 */
base::SingleThreadTaskRunner* GetIntegratedModeNetworkTaskRunner();

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_INTEGRATED_MODE_STATE_H_
