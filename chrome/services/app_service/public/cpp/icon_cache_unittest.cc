// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/services/app_service/public/cpp/icon_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep.h"

class AppsIconCacheTest : public testing::Test {
 protected:
  enum class HitOrMiss {
    kHit,
    kMiss,
  };

  using UniqueReleaser = std::unique_ptr<apps::IconLoader::Releaser>;
  static constexpr HitOrMiss kHit = HitOrMiss::kHit;
  static constexpr HitOrMiss kMiss = HitOrMiss::kMiss;

  class FakeIconLoader : public apps::IconLoader {
   public:
    int NumLoadIconFromIconKeyCalls() { return num_load_calls_; }

    void SetReturnPlaceholderIcons(bool b) { return_placeholder_icons_ = b; }

   private:
    apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override {
      return apps::mojom::IconKey::New(0, 0, 0);
    }

    std::unique_ptr<Releaser> LoadIconFromIconKey(
        apps::mojom::AppType app_type,
        const std::string& app_id,
        apps::mojom::IconKeyPtr icon_key,
        apps::mojom::IconCompression icon_compression,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon,
        apps::mojom::Publisher::LoadIconCallback callback) override {
      num_load_calls_++;

      auto iv = apps::mojom::IconValue::New();
      if (icon_compression == apps::mojom::IconCompression::kUncompressed) {
        iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
        iv->uncompressed =
            gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(1, 1), 1.0f));
        iv->is_placeholder_icon = return_placeholder_icons_;
      }

      std::move(callback).Run(std::move(iv));
      return nullptr;
    }

    int num_load_calls_ = 0;
    bool return_placeholder_icons_ = false;
  };

  UniqueReleaser LoadIcon(apps::IconLoader* loader,
                          FakeIconLoader* fake,
                          const std::string& app_id,
                          HitOrMiss expect_hom,
                          bool allow_placeholder_icon = false) {
    static constexpr auto app_type = apps::mojom::AppType::kWeb;
    static constexpr auto icon_compression =
        apps::mojom::IconCompression::kUncompressed;
    static constexpr int32_t size_hint_in_dip = 1;

    int before = fake->NumLoadIconFromIconKeyCalls();

    UniqueReleaser releaser =
        loader->LoadIcon(app_type, app_id, icon_compression, size_hint_in_dip,
                         allow_placeholder_icon, base::DoNothing());

    int after = fake->NumLoadIconFromIconKeyCalls();
    HitOrMiss actual_hom = (after == before) ? kHit : kMiss;
    EXPECT_EQ(expect_hom, actual_hom);

    return releaser;
  }

  void TestBasics(apps::IconCache::GarbageCollectionPolicy gc_policy) {
    FakeIconLoader fake;
    apps::IconCache cache(&fake, gc_policy);

    UniqueReleaser a0 = LoadIcon(&cache, &fake, "apricot", kMiss);
    a0.reset();

    UniqueReleaser b0 = LoadIcon(&cache, &fake, "banana", kMiss);
    UniqueReleaser b1 = LoadIcon(&cache, &fake, "banana", kHit);
    b0.reset();
    b1.reset();

    UniqueReleaser c0 = LoadIcon(&cache, &fake, "cherry", kMiss);
    UniqueReleaser c1 = LoadIcon(&cache, &fake, "cherry", kHit);
    UniqueReleaser c2 = LoadIcon(&cache, &fake, "cherry", kHit);
    c2.reset();
    c1.reset();

    UniqueReleaser d0 = LoadIcon(&cache, &fake, "durian", kMiss);
    d0.reset();

    UniqueReleaser c3 = LoadIcon(&cache, &fake, "cherry", kHit);
    c3.reset();

    if (gc_policy == apps::IconCache::GarbageCollectionPolicy::kExplicit) {
      cache.SweepReleasedIcons();
    }

    UniqueReleaser c4 = LoadIcon(&cache, &fake, "cherry", kHit);
    c4.reset();
    c0.reset();

    if (gc_policy == apps::IconCache::GarbageCollectionPolicy::kExplicit) {
      cache.SweepReleasedIcons();
    }

    UniqueReleaser c5 = LoadIcon(&cache, &fake, "cherry", kMiss);
    c5.reset();
  }

  void TestPlaceholder(apps::IconCache::GarbageCollectionPolicy gc_policy) {
    FakeIconLoader fake;
    apps::IconCache cache(&fake, gc_policy);
    bool allow_placeholder_icon;

    fake.SetReturnPlaceholderIcons(true);

    allow_placeholder_icon = true;
    UniqueReleaser f0 =
        LoadIcon(&cache, &fake, "fig", kMiss, allow_placeholder_icon);

    fake.SetReturnPlaceholderIcons(false);

    // The next LoadIcon call is a kMiss, even though there is a cache entry,
    // because the cache entry holds a placeholder icon, but we have
    // allow_placeholder_icon == false.
    //
    // A side effect of the next LoadIcon call is to prime the cache with the
    // real (non-placeholder) icon.

    allow_placeholder_icon = false;
    UniqueReleaser f1 =
        LoadIcon(&cache, &fake, "fig", kMiss, allow_placeholder_icon);

    // The next two LoadIcons are all both kHit's. The real icon can be served
    // from the cache, regardless of allow_placeholder_icon's value.

    allow_placeholder_icon = false;
    UniqueReleaser f2 =
        LoadIcon(&cache, &fake, "fig", kHit, allow_placeholder_icon);

    allow_placeholder_icon = true;
    UniqueReleaser f3 =
        LoadIcon(&cache, &fake, "fig", kHit, allow_placeholder_icon);
  }

  void TestAfterZeroRefcount(
      apps::IconCache::GarbageCollectionPolicy gc_policy) {
    FakeIconLoader fake;
    apps::IconCache cache(&fake, gc_policy);

    UniqueReleaser w0 = LoadIcon(&cache, &fake, "watermelon", kMiss);
    w0.reset();

    // We now have a zero ref-count. Whether the next LoadIcon call is kHit or
    // kMiss depends on our gc_policy.

    HitOrMiss expect_hom;
    switch (gc_policy) {
      case apps::IconCache::GarbageCollectionPolicy::kEager:
        expect_hom = kMiss;
        break;
      case apps::IconCache::GarbageCollectionPolicy::kExplicit:
        expect_hom = kHit;
        break;
    }

    UniqueReleaser w1 = LoadIcon(&cache, &fake, "watermelon", expect_hom);
    w1.reset();

    // Once again, we have a zero ref-count, but for a kExplicit gc_policy, we
    // also explicitly SweepReleasedIcons(), so the next LoadIcon call should
    // get kMiss.

    if (gc_policy == apps::IconCache::GarbageCollectionPolicy::kExplicit) {
      cache.SweepReleasedIcons();
    }

    UniqueReleaser w2 = LoadIcon(&cache, &fake, "watermelon", kMiss);
    w2.reset();
  }
};

TEST_F(AppsIconCacheTest, Eager) {
  static constexpr apps::IconCache::GarbageCollectionPolicy gc_policy =
      apps::IconCache::GarbageCollectionPolicy::kEager;

  TestBasics(gc_policy);
  TestPlaceholder(gc_policy);
  TestAfterZeroRefcount(gc_policy);
}

TEST_F(AppsIconCacheTest, Explicit) {
  static constexpr apps::IconCache::GarbageCollectionPolicy gc_policy =
      apps::IconCache::GarbageCollectionPolicy::kExplicit;

  TestBasics(gc_policy);
  TestPlaceholder(gc_policy);
  TestAfterZeroRefcount(gc_policy);
}
