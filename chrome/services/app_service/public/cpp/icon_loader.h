// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_LOADER_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_LOADER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps {

// An abstract class for something that can load App Service icons, either
// directly or by wrapping another IconLoader.
class IconLoader {
 public:
  // An RAII-style object that, when destroyed, runs |closure|.
  //
  // For example, that |closure| can inform an IconLoader that an icon is no
  // longer actively used by whoever held this Releaser (an object returned by
  // IconLoader::LoadIconFromIconKey). This is merely advisory: the IconLoader
  // is free to ignore the Releaser-was-destroyed hint and to e.g. keep any
  // cache entries alive for a longer or shorter time.
  //
  // These can be chained, so that |this| is the head of a linked list of
  // Releaser's. Destroying the head will destroy the rest of the list.
  //
  // Destruction must happen on the same sequence (in the
  // base/sequence_checker.h sense) as the LoadIcon or LoadIconFromIconKey call
  // that returned |this|.
  class Releaser {
   public:
    Releaser(std::unique_ptr<Releaser> next, base::OnceClosure closure);
    virtual ~Releaser();

   private:
    std::unique_ptr<Releaser> next_;
    base::OnceClosure closure_;

    DISALLOW_COPY_AND_ASSIGN(Releaser);
  };

  IconLoader();
  virtual ~IconLoader();

  // Looks up the IconKey for the given app ID.
  virtual apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) = 0;

  // This can return nullptr, meaning that the IconLoader does not track when
  // the icon is no longer actively used by the caller.
  virtual std::unique_ptr<Releaser> LoadIconFromIconKey(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconKeyPtr icon_key,
      apps::mojom::IconCompression icon_compression,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback) = 0;

  // Convenience method that calls "LoadIconFromIconKey(app_type, app_id,
  // GetIconKey(app_id), etc)".
  std::unique_ptr<Releaser> LoadIcon(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconCompression icon_compression,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback);

 protected:
  // A struct containing the arguments (other than the callback) to
  // Loader::LoadIconFromIconKey, including a flattened apps::mojom::IconKey.
  //
  // It implements operator<, so that it can be the "K" in a "map<K, V>".
  //
  // Only IconLoader subclasses (i.e. implementations), not IconLoader's
  // callers, are expected to refer to a Key.
  class Key {
   public:
    apps::mojom::AppType app_type_;
    std::string app_id_;
    // apps::mojom::IconKey fields.
    uint64_t timeline_;
    int32_t resource_id_;
    uint32_t icon_effects_;
    // Other fields.
    apps::mojom::IconCompression icon_compression_;
    int32_t size_hint_in_dip_;
    bool allow_placeholder_icon_;

    Key(apps::mojom::AppType app_type,
        const std::string& app_id,
        const apps::mojom::IconKeyPtr& icon_key,
        apps::mojom::IconCompression icon_compression,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon);

    Key(const Key& other);

    bool operator<(const Key& that) const;
  };
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_LOADER_H_
