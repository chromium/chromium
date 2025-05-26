// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_id_traits.h"

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

TEST(TabsStripServiceMojoTraitsTest, Convert) {
  TabId original(TabId::Type::kCollection, "super_secret_id");

  auto serialized = mojom::TabId::Serialize(&original);

  TabId deserialized;
  mojom::TabId::Deserialize(serialized, &deserialized);

  ASSERT_TRUE(original == deserialized);
}

}  // namespace
}  // namespace tabs_api
