// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_global_state.h"

#import <Foundation/Foundation.h>

#include <utility>

#include "base/callback.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/init/ios_global_state.h"
#include "ios/web/public/init/ios_global_state_configuration.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

void InitializeOnMainThread() {
  // This method must be called once from the main thread.
  DCHECK_EQ([NSThread currentThread], [NSThread mainThread]);

  ios_global_state::CreateParams create_params;
  create_params.install_at_exit_manager = true;
  ios_global_state::Create(create_params);
  ios_global_state::StartThreadPool();

  ios_global_state::BuildSingleThreadTaskExecutor();
  ios_global_state::CreateNetworkChangeNotifier();
}

}  // namespace

namespace cronet {

bool OnInitThread() {
  return [NSThread isMainThread] == YES;
}

void PostTaskToInitThread(const base::Location& posted_from,
                          base::OnceClosure task) {
  __block base::OnceClosure block_task(std::move(task));
  if (!OnInitThread()) {
    dispatch_async(dispatch_get_main_queue(), ^(void) {
      std::move(block_task).Run();
    });
  } else {
    std::move(block_task).Run();
  }
}

void EnsureInitialized() {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    if (!OnInitThread()) {
      dispatch_sync(dispatch_get_main_queue(), ^(void) {
        InitializeOnMainThread();
      });
    } else {
      InitializeOnMainThread();
    }
  });
}

std::unique_ptr<net::ProxyConfigService> CreateProxyConfigService(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  return nullptr;
}

std::unique_ptr<net::ProxyResolutionService> CreateProxyResolutionService(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::NetLog* net_log) {
  return nullptr;
}

// Creates default User-Agent request value, combining optional
// |partial_user_agent| with system-dependent values.
std::string CreateDefaultUserAgent(const std::string& partial_user_agent) {
  return web::BuildMobileUserAgent(partial_user_agent);
}

void SetNetworkThreadPriorityOnNetworkThread(double priority) {
  DCHECK_LE(priority, 1.0);
  DCHECK_GE(priority, 0.0);
  if (priority >= 0.0 && priority <= 1.0)
    [NSThread setThreadPriority:priority];
}

}  // namespace cronet
