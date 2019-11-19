// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_CACHE_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_CACHE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/services/app_service/public/cpp/icon_loader.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

// An IconLoader that caches the apps::mojom::IconCompression::kUncompressed
// results of another (wrapped) IconLoader.
class IconCache : public IconLoader {
 public:
  // What triggers dropping no-longer-used icons from the cache.
  //
  // If unsure of which one to use, kEager is a safe choice, with little
  // overhead above not having an icon cache at all.
  enum class GarbageCollectionPolicy {
    // kEager means that we drop icons as soon as their ref-count hits zero
    // (i.e. all the IconLoader::Releaser's returned by LoadIconFromIconKey
    // have been destroyed).
    //
    // This minimizes the overall memory cost of the cache. Only icons that are
    // still actively used stay alive in the cache.
    //
    // On the other hand, this can result in more cache misses than other
    // policies. For example, suppose that some UI starts with a widget showing
    // the "foo" app icon. In response to user input, the UI destroys that
    // widget and then creates a new widget to show the same "foo" app icon.
    // With a kEager garbage collection policy, that freshly created widget
    // might not get a cache hit, if the icon's ref-count hits zero in between
    // the two widgets' destruction and creation.
    kEager,

    // kExplicit means that icons can remain in the cache, even if their
    // ref-count hits zero. Instead, explicit calls to SweepReleasedIcons are
    // needed to clear cache entries.
    //
    // This can use more memory than kEager, but it can also provide a cache
    // hit in the "destroy and then create" example described above.
    //
    // On the other hand, it requires more effort and more thought from the
    // programmer. They need to make additional calls (to SweepReleasedIcons),
    // so they can't just drop an IconCache in transparently. The programmer
    // also needs to think about when is a good time to make those calls. Too
    // frequent, and you get extra complexity for not much more benefit than
    // using kEager. Too infrequent, and you have the memory cost of keeping
    // unused icons around.
    //
    // All together, kExplicit might not be the best policy for e.g. a
    // process-wide icon cache with many clients, each with different usage
    // patterns.
    kExplicit,
  };

  IconCache(IconLoader* wrapped_loader, GarbageCollectionPolicy gc_policy);
  ~IconCache() override;

  // IconLoader overrides.
  apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
  std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconKeyPtr icon_key,
      apps::mojom::IconCompression icon_compression,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback) override;

  // A hint that now is a good time to garbage-collect any icons that are not
  // actively held.
  void SweepReleasedIcons();

 private:
  class Value {
   public:
    gfx::ImageSkia image_;
    bool is_placeholder_icon_;
    uint64_t ref_count_;

    Value();

    apps::mojom::IconValuePtr AsIconValue();
  };

  void Update(const IconLoader::Key&, const apps::mojom::IconValue&);
  void OnLoadIcon(IconLoader::Key,
                  apps::mojom::Publisher::LoadIconCallback,
                  apps::mojom::IconValuePtr);
  void OnRelease(IconLoader::Key);

  std::map<IconLoader::Key, Value> map_;
  IconLoader* wrapped_loader_;
  GarbageCollectionPolicy gc_policy_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IconCache> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IconCache);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_CACHE_H_
