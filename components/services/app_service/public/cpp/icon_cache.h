// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_CACHE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_CACHE_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

// This is used for logging, so do not remove or reorder existing entries.
enum class IconLoadingMethod {
  kFromCache = 0,
  kViaMojomCall = 1,
  kViaNonMojomCall = 2,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kViaNonMojomCall,
};

// Records metrics when loading icons.
void RecordIconLoadMethodMetrics(IconLoadingMethod icon_loading_method);

// An IconLoader that caches the apps::mojom::IconType::kUncompressed
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

  IconCache(const IconCache&) = delete;
  IconCache& operator=(const IconCache&) = delete;

  ~IconCache() override;

  // IconLoader overrides.
  std::optional<IconKey> GetIconKey(const std::string& id) override;
  std::unique_ptr<Releaser> LoadIconFromIconKey(
      const std::string& id,
      const IconKey& icon_key,
      IconType icon_type,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::LoadIconCallback callback) override;

  // A hint that now is a good time to garbage-collect any icons that are not
  // actively held.
  void SweepReleasedIcons();

  void RemoveIcon(const std::string& id);

 private:
  class Value {
   public:
    gfx::ImageSkia image_;
    bool is_placeholder_icon_;
    uint64_t ref_count_;

    Value();

    IconValuePtr AsIconValue(IconType icon_type);
  };

  void Update(const IconLoader::Key& key, const IconValue& icon_value);
  void OnLoadIcon(const IconLoader::Key& key,
                  apps::LoadIconCallback callback,
                  IconValuePtr icon_value);

  void OnRelease(IconLoader::Key);

  std::map<IconLoader::Key, Value> map_;
  raw_ptr<IconLoader, DanglingUntriaged> wrapped_loader_;
  GarbageCollectionPolicy gc_policy_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IconCache> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_CACHE_H_
