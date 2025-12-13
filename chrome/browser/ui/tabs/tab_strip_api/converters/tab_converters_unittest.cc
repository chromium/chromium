// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider.h"
#include "url/gurl.h"

namespace tabs_api::converters {
namespace {

class FakeTabCollection : public tabs::TabCollection {
 public:
  explicit FakeTabCollection(Type type) : TabCollection(type, {}, true) {}
  ~FakeTabCollection() override = default;
};

TEST(TabStripServiceConverters, ConvertTab) {
  tabs::TabHandle handle(888);
  ui::ColorProvider color_provider;
  TabRendererData data;
  data.visible_url = GURL("http://nowhere");
  data.title = std::u16string(u"title");

  auto mojo = BuildMojoTab(handle, data, color_provider,
                           {
                               .is_active = true,
                               .is_selected = true,
                           });

  ASSERT_EQ("888", mojo->id.Id());
  ASSERT_EQ(NodeId::Type::kContent, mojo->id.Type());
  ASSERT_EQ(GURL("http://nowhere"), mojo->url);
  ASSERT_EQ("title", mojo->title);
  ASSERT_TRUE(mojo->is_active);
  ASSERT_TRUE(mojo->is_selected);
}

TEST(TabStripServiceConverters, ConvertTabCollection) {
  FakeTabCollection collection(tabs::TabCollection::Type::TABSTRIP);
  const std::string expected_id =
      base::NumberToString(collection.GetHandle().raw_value());
  auto mojo = BuildMojoTabCollectionData(collection.GetHandle());

  ASSERT_TRUE(mojo->is_tab_strip());

  const auto& tab_strip = mojo->get_tab_strip();
  ASSERT_EQ(expected_id, tab_strip->id.Id());
  ASSERT_EQ(NodeId::Type::kCollection, tab_strip->id.Type());
}

}  // namespace
}  // namespace tabs_api::converters
