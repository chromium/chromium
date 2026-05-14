// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/common/extensions/extension_test_util.h"
#include "extensions/common/extensions_client.h"
#endif

class ChromeContentRendererClientTest : public testing::Test {
 public:
  void SetUp() override {
    // Ensure that this looks like the renderer process based on the command
    // line.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kProcessType, switches::kRendererProcess);
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
TEST_F(ChromeContentRendererClientTest, ExtensionsClientInitialized) {
  auto* extensions_client = extensions::ExtensionsClient::Get();
  ASSERT_TRUE(extensions_client);

  // Ensure that the availability map is initialized correctly.
  const auto& map =
      extensions_client->GetFeatureDelegatedAvailabilityCheckMap();
  EXPECT_TRUE(!map.empty());
  for (const char* feature :
       extension_test_util::GetExpectedDelegatedFeaturesForTest()) {
    EXPECT_EQ(1u, map.count(feature)) << feature;
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
