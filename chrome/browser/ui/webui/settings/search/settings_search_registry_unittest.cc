// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_search {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

std::unique_ptr<SettingsSearchItem> MakeItem(mojom::SettingId id,
                                             const std::u16string& title) {
  return std::make_unique<SettingsSearchItem>(
      id, "/appearance", title, std::vector<std::u16string>{u"Appearance"},
      std::vector<std::u16string>());
}

std::vector<std::u16string> GetTitles(const SettingsSearchRegistry& registry) {
  std::vector<std::u16string> titles;
  for (const raw_ptr<SettingsSearchItem>& item : registry.GetItems()) {
    titles.push_back(item->GetTitle());
  }
  return titles;
}

class UpdateCounter : public SettingsSearchRegistry::Observer {
 public:
  explicit UpdateCounter(SettingsSearchRegistry* registry) {
    observation_.Observe(registry);
  }

  int count() const { return count_; }

  // SettingsSearchRegistry::Observer:
  void OnRegistryUpdated() override { ++count_; }

 private:
  int count_ = 0;
  base::ScopedObservation<SettingsSearchRegistry,
                          SettingsSearchRegistry::Observer>
      observation_{this};
};

// Verifies that OnRegistryUpdated() runs after `items_` is updated and before
// any replaced or removed item is destroyed, so observers holding raw_ptr to
// items can clear their pointers in OnRegistryUpdated() without dangling.
class ItemLifetimeObserver : public SettingsSearchRegistry::Observer {
 public:
  ItemLifetimeObserver(SettingsSearchRegistry* registry,
                       const SettingsSearchItem* tracked_item)
      : registry_(registry), tracked_item_(tracked_item) {
    observation_.Observe(registry);
  }

  const std::u16string& title_during_update() const {
    return title_during_update_;
  }

  const std::vector<std::u16string>& registry_titles_during_update() const {
    return registry_titles_during_update_;
  }

  // SettingsSearchRegistry::Observer:
  void OnRegistryUpdated() override {
    ASSERT_TRUE(tracked_item_);
    title_during_update_ = tracked_item_->GetTitle();
    registry_titles_during_update_ = GetTitles(*registry_);
    tracked_item_ = nullptr;
  }

 private:
  const raw_ptr<const SettingsSearchRegistry> registry_;
  raw_ptr<const SettingsSearchItem> tracked_item_;
  std::u16string title_during_update_;
  std::vector<std::u16string> registry_titles_during_update_;
  base::ScopedObservation<SettingsSearchRegistry,
                          SettingsSearchRegistry::Observer>
      observation_{this};
};

TEST(SettingsSearchRegistryTest, AddAndRemove) {
  SettingsSearchRegistry registry;
  registry.AddItem(MakeItem(mojom::SettingId::kPageZoom, u"Page zoom"));
  registry.AddItem(MakeItem(mojom::SettingId::kFontSize, u"Font size"));

  // Ordered by id: kFontSize < kPageZoom.
  EXPECT_THAT(GetTitles(registry), ElementsAre(u"Font size", u"Page zoom"));

  registry.RemoveItem(mojom::SettingId::kFontSize);
  EXPECT_THAT(GetTitles(registry), ElementsAre(u"Page zoom"));
}

TEST(SettingsSearchRegistryTest, AddReplacesSameId) {
  SettingsSearchRegistry registry;
  registry.AddItem(MakeItem(mojom::SettingId::kPageZoom, u"Page zoom"));
  registry.AddItem(MakeItem(mojom::SettingId::kPageZoom, u"Zoom"));

  EXPECT_THAT(GetTitles(registry), ElementsAre(u"Zoom"));
}

TEST(SettingsSearchRegistryTest, RemoveUnknownIdDoesNothing) {
  SettingsSearchRegistry registry;
  registry.AddItem(MakeItem(mojom::SettingId::kPageZoom, u"Page zoom"));
  UpdateCounter counter(&registry);

  registry.RemoveItem(mojom::SettingId::kFontSize);

  EXPECT_THAT(GetTitles(registry), ElementsAre(u"Page zoom"));
  EXPECT_EQ(counter.count(), 0);
}

TEST(SettingsSearchRegistryTest, NotifiesObserversOnChange) {
  SettingsSearchRegistry registry;
  UpdateCounter counter(&registry);

  registry.AddItem(MakeItem(mojom::SettingId::kPageZoom, u"Page zoom"));
  EXPECT_EQ(counter.count(), 1);
  registry.AddItem(MakeItem(mojom::SettingId::kPageZoom, u"Zoom"));
  EXPECT_EQ(counter.count(), 2);
  registry.RemoveItem(mojom::SettingId::kPageZoom);
  EXPECT_EQ(counter.count(), 3);
}

TEST(SettingsSearchRegistryTest,
     ReplacedAndRemovedItemsStayAliveDuringOnRegistryUpdated) {
  SettingsSearchRegistry registry;
  registry.AddItem(MakeItem(mojom::SettingId::kPageZoom, u"Page zoom"));
  ASSERT_EQ(registry.GetItems().size(), 1u);

  {
    ItemLifetimeObserver observer(&registry, registry.GetItems()[0].get());
    registry.AddItem(MakeItem(mojom::SettingId::kPageZoom, u"Zoom"));
    EXPECT_EQ(observer.title_during_update(), u"Page zoom");
    EXPECT_THAT(observer.registry_titles_during_update(), ElementsAre(u"Zoom"));
  }

  {
    ItemLifetimeObserver observer(&registry, registry.GetItems()[0].get());
    registry.RemoveItem(mojom::SettingId::kPageZoom);
    EXPECT_EQ(observer.title_during_update(), u"Zoom");
    EXPECT_THAT(observer.registry_titles_during_update(), IsEmpty());
  }
}

}  // namespace

}  // namespace settings_search
