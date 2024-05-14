// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_coalescer.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep.h"

class AppsIconCoalescerTest : public testing::Test {
 protected:
  using UniqueReleaser = std::unique_ptr<apps::IconLoader::Releaser>;

  class FakeIconLoader : public apps::IconLoader {
   public:
    int NumLoadIconFromIconKeyCalls() { return num_load_calls_; }

    int NumLoadIconFromIconKeyCallsComplete() {
      return num_load_calls_ - pending_callbacks_.size();
    }

    int NumLoadIconFromIconKeyCallsPending() {
      return pending_callbacks_.size();
    }

    int NumPendingReleases() { return num_pending_releases_; }

    void SetCallBackImmediately(bool b) { call_back_immediately_ = b; }

    void CallBack(const std::string& app_id) {
      auto iter = pending_callbacks_.find(app_id);
      if (iter != pending_callbacks_.end()) {
        std::move(iter->second).Run(NewIconValuePtr());
        pending_callbacks_.erase(iter);
      } else {
        NOTREACHED_IN_MIGRATION()
            << "No pending callback for app_id=" << app_id;
      }
    }

   private:
    std::unique_ptr<Releaser> LoadIconFromIconKey(
        const std::string& id,
        const apps::IconKey& icon_key,
        apps::IconType icon_type,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::LoadIconCallback callback) override {
      num_load_calls_++;
      if (call_back_immediately_) {
        num_load_calls_complete_++;
        std::move(callback).Run(NewIconValuePtr());
      } else {
        pending_callbacks_.insert(std::make_pair(id, std::move(callback)));
      }
      num_pending_releases_++;
      return std::make_unique<IconLoader::Releaser>(
          nullptr, base::BindOnce(&FakeIconLoader::OnRelease,
                                  weak_ptr_factory_.GetWeakPtr()));
    }

    apps::IconValuePtr NewIconValuePtr() {
      auto iv = std::make_unique<apps::IconValue>();
      iv->icon_type = apps::IconType::kUncompressed;
      iv->uncompressed =
          gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
      iv->is_placeholder_icon = false;
      return iv;
    }

    void OnRelease() { num_pending_releases_--; }

    bool call_back_immediately_ = false;
    int num_load_calls_ = 0;
    int num_load_calls_complete_ = 0;
    int num_pending_releases_ = 0;
    std::multimap<std::string, apps::LoadIconCallback> pending_callbacks_;

    base::WeakPtrFactory<FakeIconLoader> weak_ptr_factory_{this};
  };

  UniqueReleaser LoadIcon(apps::IconLoader* loader,
                          const std::string& app_id,
                          int* counter,
                          int delta) {
    return loader->LoadIcon(
        app_id, apps::IconType::kUncompressed,
        /*size_hint_in_dip=*/1, /*allow_placeholder_icon=*/false,
        base::BindOnce([](int* counter, int delta,
                          apps::IconValuePtr icon) { *counter += delta; },
                       counter, delta));
  }
};

TEST_F(AppsIconCoalescerTest, CallBackImmediately) {
  FakeIconLoader fake;
  fake.SetCallBackImmediately(true);
  apps::IconCoalescer coalescer(&fake);
  int counter = 0;

  UniqueReleaser releaser = LoadIcon(&coalescer, "the_app_id", &counter, 1000);

  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(0, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(1000, counter);
  EXPECT_EQ(1, fake.NumPendingReleases());

  releaser.reset();

  EXPECT_EQ(0, fake.NumPendingReleases());
}

TEST_F(AppsIconCoalescerTest, CallBackDelayedAndAfterRelease) {
  FakeIconLoader fake;
  apps::IconCoalescer coalescer(&fake);
  int counter = 0;

  UniqueReleaser releaser = LoadIcon(&coalescer, "the_app_id", &counter, 1000);

  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(0, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(0, counter);
  EXPECT_EQ(1, fake.NumPendingReleases());

  fake.CallBack("the_app_id");

  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(0, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(1000, counter);
  EXPECT_EQ(1, fake.NumPendingReleases());

  releaser.reset();

  EXPECT_EQ(0, fake.NumPendingReleases());
}

TEST_F(AppsIconCoalescerTest, CallBackDelayedAndBeforeRelease) {
  FakeIconLoader fake;
  apps::IconCoalescer coalescer(&fake);
  int counter = 0;

  UniqueReleaser releaser = LoadIcon(&coalescer, "the_app_id", &counter, 1000);

  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(0, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(0, counter);
  EXPECT_EQ(1, fake.NumPendingReleases());

  // Even though we release our claim on the outer IconLoader::Releaser (from
  // the IconCoalescer), the inner IconLoader::Releaser (from the
  // FakeIconLoader) isn't released while the callback's still pending.
  releaser.reset();

  EXPECT_EQ(1, fake.NumPendingReleases());

  fake.CallBack("the_app_id");

  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(0, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(1000, counter);
  EXPECT_EQ(0, fake.NumPendingReleases());
}

TEST_F(AppsIconCoalescerTest, MultipleAppIDs) {
  FakeIconLoader fake;
  apps::IconCoalescer coalescer(&fake);
  int ant_counter = 0;
  int bat_counter = 0;
  int cat_counter = 0;
  int dog_counter = 0;
  int emu_counter = 0;

  UniqueReleaser a1 = LoadIcon(&coalescer, "ant", &ant_counter, 10);
  UniqueReleaser b1 = LoadIcon(&coalescer, "bat", &bat_counter, 100);
  UniqueReleaser c1 = LoadIcon(&coalescer, "cat", &cat_counter, 1000);

  EXPECT_EQ(3, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(0, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(3, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(0, ant_counter);
  EXPECT_EQ(0, bat_counter);
  EXPECT_EQ(0, cat_counter);
  EXPECT_EQ(0, dog_counter);
  EXPECT_EQ(0, emu_counter);

  UniqueReleaser c2 = LoadIcon(&coalescer, "cat", &cat_counter, 2000);
  UniqueReleaser d1 = LoadIcon(&coalescer, "dog", &dog_counter, 10000);
  UniqueReleaser c4 = LoadIcon(&coalescer, "cat", &cat_counter, 4000);
  UniqueReleaser b2 = LoadIcon(&coalescer, "bat", &bat_counter, 200);

  EXPECT_EQ(4, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(0, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(4, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(0, ant_counter);
  EXPECT_EQ(0, bat_counter);
  EXPECT_EQ(0, cat_counter);
  EXPECT_EQ(0, dog_counter);
  EXPECT_EQ(0, emu_counter);

  fake.CallBack("ant");
  fake.CallBack("cat");

  EXPECT_EQ(4, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(2, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(2, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(10, ant_counter);
  EXPECT_EQ(0, bat_counter);
  EXPECT_EQ(7000, cat_counter);
  EXPECT_EQ(0, dog_counter);
  EXPECT_EQ(0, emu_counter);

  UniqueReleaser a4 = LoadIcon(&coalescer, "ant", &ant_counter, 40);
  UniqueReleaser b4 = LoadIcon(&coalescer, "bat", &bat_counter, 400);
  UniqueReleaser a2 = LoadIcon(&coalescer, "ant", &ant_counter, 20);

  EXPECT_EQ(5, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(2, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(3, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(10, ant_counter);
  EXPECT_EQ(0, bat_counter);
  EXPECT_EQ(7000, cat_counter);
  EXPECT_EQ(0, dog_counter);
  EXPECT_EQ(0, emu_counter);

  // 5 NumLoadIconFromIconKeyCalls, without any releases, means 5
  // NumPendingReleases: {a1}, {a2, a4}, {b*}, {c*} and {d*}. The "a"s are two
  // different groups, as they are separated by a `fake.Callback("ant")` line.
  EXPECT_EQ(5, fake.NumPendingReleases());
  fake.CallBack("ant");

  // We treat the "b"s differently, releasing them (resetting the
  // UniqueReleaser, aka unique_ptr<IconLoader::Releaser>) *before* (not
  // *after) we tickle fake.CallBack. Still, the inner-most releaser isn't let
  // go until both (1) all outer releasers are dropped and (2) the inner
  // IconLoader has actually called back.
  EXPECT_EQ(5, fake.NumPendingReleases());
  b1.reset();
  b2.reset();
  b4.reset();
  EXPECT_EQ(5, fake.NumPendingReleases());
  fake.CallBack("bat");
  EXPECT_EQ(4, fake.NumPendingReleases());

  EXPECT_EQ(5, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(4, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(70, ant_counter);
  EXPECT_EQ(700, bat_counter);
  EXPECT_EQ(7000, cat_counter);
  EXPECT_EQ(0, dog_counter);
  EXPECT_EQ(0, emu_counter);

  // Even though we configure the fake to call back immediately, the next two
  // "dog" calls still wait for the previous (pending) "dog" call. The next
  // three "emu" calls lead to three (immediate) calls on the fake.
  fake.SetCallBackImmediately(true);
  EXPECT_EQ(4, fake.NumPendingReleases());
  UniqueReleaser d2 = LoadIcon(&coalescer, "dog", &dog_counter, 20000);
  UniqueReleaser d4 = LoadIcon(&coalescer, "dog", &dog_counter, 40000);
  EXPECT_EQ(4, fake.NumPendingReleases());
  UniqueReleaser e1 = LoadIcon(&coalescer, "emu", &emu_counter, 100000);
  UniqueReleaser e2 = LoadIcon(&coalescer, "emu", &emu_counter, 200000);
  UniqueReleaser e4 = LoadIcon(&coalescer, "emu", &emu_counter, 400000);
  EXPECT_EQ(7, fake.NumPendingReleases());

  EXPECT_EQ(8, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(7, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(1, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(70, ant_counter);
  EXPECT_EQ(700, bat_counter);
  EXPECT_EQ(7000, cat_counter);
  EXPECT_EQ(0, dog_counter);
  EXPECT_EQ(700000, emu_counter);

  fake.CallBack("dog");

  EXPECT_EQ(8, fake.NumLoadIconFromIconKeyCalls());
  EXPECT_EQ(8, fake.NumLoadIconFromIconKeyCallsComplete());
  EXPECT_EQ(0, fake.NumLoadIconFromIconKeyCallsPending());
  EXPECT_EQ(70, ant_counter);
  EXPECT_EQ(700, bat_counter);
  EXPECT_EQ(7000, cat_counter);
  EXPECT_EQ(70000, dog_counter);
  EXPECT_EQ(700000, emu_counter);

  // As mentioned above, {a1} and {a2, a4} are different groups.
  EXPECT_EQ(7, fake.NumPendingReleases());
  a1.reset();
  EXPECT_EQ(6, fake.NumPendingReleases());
  a2.reset();
  EXPECT_EQ(6, fake.NumPendingReleases());
  a4.reset();
  EXPECT_EQ(5, fake.NumPendingReleases());

  // {c*} and {d*} are each one group, but {e1}, {e2} and {e4} are three
  // separate groups.
  EXPECT_EQ(5, fake.NumPendingReleases());
  c1.reset();
  EXPECT_EQ(5, fake.NumPendingReleases());
  c2.reset();
  EXPECT_EQ(5, fake.NumPendingReleases());
  c4.reset();
  EXPECT_EQ(4, fake.NumPendingReleases());
  d1.reset();
  EXPECT_EQ(4, fake.NumPendingReleases());
  d2.reset();
  EXPECT_EQ(4, fake.NumPendingReleases());
  d4.reset();
  EXPECT_EQ(3, fake.NumPendingReleases());
  e1.reset();
  EXPECT_EQ(2, fake.NumPendingReleases());
  e2.reset();
  EXPECT_EQ(1, fake.NumPendingReleases());
  e4.reset();
  EXPECT_EQ(0, fake.NumPendingReleases());
}
