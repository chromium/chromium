// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform_api_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "chromeos/services/assistant/public/cpp/features.h"

namespace chromeos {
namespace assistant {

////////////////////////////////////////////////////////////////////////////////
// PlatformApiImpl
////////////////////////////////////////////////////////////////////////////////

PlatformApiImpl::PlatformApiImpl(
    chromeos::libassistant::mojom::PlatformDelegate* platform_delegate,
    PowerManagerClient* power_manager_client,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner) {}

PlatformApiImpl::~PlatformApiImpl() = default;

}  // namespace assistant
}  // namespace chromeos
