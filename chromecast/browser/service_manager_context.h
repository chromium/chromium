// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_MANAGER_CONTEXT_H_
#define CHROMECAST_BROWSER_SERVICE_MANAGER_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {
class Connector;
}

namespace chromecast {

namespace shell {
class CastContentBrowserClient;
}

// ServiceManagerContext manages the browser's connection to the ServiceManager,
// hosting a new in-process ServiceManagerContext if the browser was not
// launched from an external one.
class ServiceManagerContext {
 public:
  static const char kBrowserServiceName[];

  ServiceManagerContext(
      shell::CastContentBrowserClient* cast_content_browser_client,
      scoped_refptr<base::SingleThreadTaskRunner>
          service_manager_thread_task_runner);

  ServiceManagerContext(const ServiceManagerContext&) = delete;
  ServiceManagerContext& operator=(const ServiceManagerContext&) = delete;

  ~ServiceManagerContext();

  // Returns a service_manager::Connector that can be used on the IO thread.
  static service_manager::Connector* GetConnectorForIOThread();

  // Shutdowns the ServiceManager and the connections to the ServiceManager.
  void ShutDown();

 private:
  class InProcessServiceManagerContext;

  void RunServiceInstance(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);

  shell::CastContentBrowserClient* const cast_content_browser_client_;
  scoped_refptr<base::SingleThreadTaskRunner>
      service_manager_thread_task_runner_;
  scoped_refptr<InProcessServiceManagerContext> in_process_context_;
  base::WeakPtrFactory<ServiceManagerContext> weak_ptr_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_MANAGER_CONTEXT_H_
