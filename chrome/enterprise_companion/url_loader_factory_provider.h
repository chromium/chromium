// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_URL_LOADER_FACTORY_PROVIDER_H_
#define CHROME_ENTERPRISE_COMPANION_URL_LOADER_FACTORY_PROVIDER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/event_logger.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace enterprise_companion {

// base::SequenceBound-compatible interface to create
// PendingSharedURLLoaderFactory instances on an IO thread which may be
// materialized on other threads. This class must be used on a single sequence
// with an IO message pump.
class URLLoaderFactoryProvider {
 public:
  virtual ~URLLoaderFactoryProvider() = default;

  virtual std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetPendingURLLoaderFactory() = 0;
};

// Creates a URLLoaderFactoryProvider which services network requests
// in-process. If `event_logger_cookie_handler` is valid, it will be owned and
// started to manage the population and persistence of the event logging cookie.
// If `pending_receiver` is valid, it is bound to the underlying implementation,
// allowing an out-of-process caller to use it. If `pending_receiver` is valid,
// `disconnect_handler` will be run if the connection is dropped.
base::SequenceBound<URLLoaderFactoryProvider>
CreateInProcessUrlLoaderFactoryProvider(
    scoped_refptr<base::SingleThreadTaskRunner> net_thread_runner,
    base::SequenceBound<EventLoggerCookieHandler> event_logger_cookie_handler,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver =
        {},
    base::OnceClosure on_disconnect_callback = base::DoNothing());

#if BUILDFLAG(IS_MAC)
// Creates a URLLoaderFactoryProvider which delegates network requests to a
// remote process.
base::SequenceBound<URLLoaderFactoryProvider>
CreateUrlLoaderFactoryProviderProxy(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote,
    base::OnceClosure on_disconnect_callback);

// Launches and returns a connection to the embedded net worker process. Returns
// a null SequenceBound on error.
base::SequenceBound<URLLoaderFactoryProvider> CreateOutOfProcessNetWorker(
    base::OnceClosure on_disconnect_callback);
#endif

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_URL_LOADER_FACTORY_PROVIDER_H_
