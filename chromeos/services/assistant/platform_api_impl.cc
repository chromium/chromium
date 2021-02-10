// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform_api_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/utils.h"
#include "libassistant/shared/public/assistant_export.h"
#include "libassistant/shared/public/platform_api.h"
#include "libassistant/shared/public/platform_factory.h"

using assistant_client::FileProvider;
using assistant_client::NetworkProvider;
using assistant_client::PlatformApi;

namespace chromeos {
namespace assistant {

////////////////////////////////////////////////////////////////////////////////
// PlatformApiImpl
////////////////////////////////////////////////////////////////////////////////

PlatformApiImpl::PlatformApiImpl(
    chromeos::libassistant::mojom::PlatformDelegate* platform_delegate,
    PowerManagerClient* power_manager_client,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner)
    : network_provider_(platform_delegate) {}

PlatformApiImpl::~PlatformApiImpl() = default;

FileProvider& PlatformApiImpl::GetFileProvider() {
  return file_provider_;
}

NetworkProvider& PlatformApiImpl::GetNetworkProvider() {
  return network_provider_;
}

}  // namespace assistant
}  // namespace chromeos
