// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_global_state.h"

#include "base/at_exit.h"
#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"

// This file provides minimal "stub" implementations of the Cronet global-state
// functions for the native library build, sufficient to have cronet_tests and
// cronet_unittests build.

namespace cronet {

namespace {

scoped_refptr<base::SingleThreadTaskRunner> InitializeAndCreateTaskRunner() {
// Cronet tests sets AtExitManager as part of TestSuite, so statically linked
// library is not allowed to set its own.
#if !defined(CRONET_TESTS_IMPLEMENTATION)
  ignore_result(new base::AtExitManager);
#endif

  base::FeatureList::InitializeInstance(std::string(), std::string());

  // Note that in component builds this ThreadPoolInstance will be shared with
  // the calling process, if it also depends on //base. In particular this means
  // that the Cronet test binaries must avoid initializing or shutting-down the
  // ThreadPoolInstance themselves.
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("cronet");

  return base::CreateSingleThreadTaskRunner({base::ThreadPool()});
}

base::SingleThreadTaskRunner* InitTaskRunner() {
  static scoped_refptr<base::SingleThreadTaskRunner> init_task_runner =
      InitializeAndCreateTaskRunner();
  return init_task_runner.get();
}

}  // namespace

void EnsureInitialized() {
  ignore_result(InitTaskRunner());
}

bool OnInitThread() {
  return InitTaskRunner()->BelongsToCurrentThread();
}

void PostTaskToInitThread(const base::Location& posted_from,
                          base::OnceClosure task) {
  InitTaskRunner()->PostTask(posted_from, std::move(task));
}

std::unique_ptr<net::ProxyConfigService> CreateProxyConfigService(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  return net::ProxyResolutionService::CreateSystemProxyConfigService(
      io_task_runner);
}

std::unique_ptr<net::ProxyResolutionService> CreateProxyResolutionService(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::NetLog* net_log) {
  return net::ProxyResolutionService::CreateUsingSystemProxyResolver(
      std::move(proxy_config_service), net_log);
}

std::string CreateDefaultUserAgent(const std::string& partial_user_agent) {
  return partial_user_agent;
}

void SetNetworkThreadPriorityOnNetworkThread(double priority) {
  NOTIMPLEMENTED();
}

}  // namespace cronet
