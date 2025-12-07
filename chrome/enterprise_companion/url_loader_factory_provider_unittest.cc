// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/url_loader_factory_provider.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/ipc_support.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_companion {

class URLLoaderFactoryProviderTest : public ::testing::Test {
 private:
  base::test::TaskEnvironment environment_;
  ScopedIPCSupportWrapper ipc_support_;
};

#if BUILDFLAG(IS_MAC)
TEST_F(URLLoaderFactoryProviderTest, ProxyDisconnectHandler) {
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver;
  base::RunLoop run_loop;
  base::SequenceBound<URLLoaderFactoryProvider> url_loader_factory_provider =
      CreateUrlLoaderFactoryProviderProxy(
          base::SequencedTaskRunner::GetCurrentDefault(),
          receiver.InitWithNewPipeAndPassRemote(), run_loop.QuitClosure());
  receiver.reset();
  run_loop.Run();
}
#endif

TEST_F(URLLoaderFactoryProviderTest, StubDisconnectHandler) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
  base::RunLoop run_loop;
  base::SequenceBound<URLLoaderFactoryProvider> url_loader_factory_provider =
      CreateInProcessUrlLoaderFactoryProvider(
          base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()}),
          /*event_logger_cookie_handler=*/{},
          remote.InitWithNewPipeAndPassReceiver(), run_loop.QuitClosure());
  remote.reset();
  run_loop.Run();
}

}  // namespace enterprise_companion
