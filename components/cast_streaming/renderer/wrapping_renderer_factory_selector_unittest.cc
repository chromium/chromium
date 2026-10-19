// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/public/wrapping_renderer_factory_selector.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "components/cast_streaming/renderer/public/resource_provider.h"
#include "components/cast_streaming/renderer/public/resource_provider_factory.h"
#include "media/base/mock_filters.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_streaming {

TEST(WrappingRendererFactorySelectorTest,
     UsesBaseFactoryAfterRendererControllerDisconnect) {
  base::test::SingleThreadTaskEnvironment task_environment;
  std::unique_ptr<ResourceProvider> resource_provider =
      CreateResourceProvider();

  mojo::AssociatedRemote<mojom::RendererController> remote;
  resource_provider->GetRendererControllerBinder().Run(
      remote.BindNewEndpointAndPassReceiver());
  remote.reset();
  base::RunLoop().RunUntilIdle();

  WrappingRendererFactorySelector selector(resource_provider.get());
  auto base_factory = std::make_unique<media::MockRendererFactory>();
  media::RendererFactory* base_factory_ptr = base_factory.get();
  selector.AddBaseFactory(media::RendererType::kTest, std::move(base_factory));

  EXPECT_EQ(selector.GetCurrentFactory(), base_factory_ptr);
}

}  // namespace cast_streaming
