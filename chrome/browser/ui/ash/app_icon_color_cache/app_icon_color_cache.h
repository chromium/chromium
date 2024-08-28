// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_ICON_COLOR_CACHE_APP_ICON_COLOR_CACHE_H_
#define CHROME_BROWSER_UI_ASH_APP_ICON_COLOR_CACHE_APP_ICON_COLOR_CACHE_H_

#include <map>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "third_party/skia/include/core/SkColor.h"

class Profile;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class IconColor;

// This feature controls whether `IconColor`s are stored in both persistent
// storage (via profile preferences) and memory for faster access. When
// disabled, the feature clears the cache of `IconColor`.
BASE_DECLARE_FEATURE(kEnablePersistentAshIconColorCache);

// Backed by the prefs cache. Created lazily for a profile when first accessed.
// Cache entries are set when app icons are set. Cache entries are removed when:
// 1. An app's icon has a new version; OR
// 2. An app is uninstalled.
class AppIconColorCache : public ProfileObserver {
 public:
  explicit AppIconColorCache(Profile* profile);
  AppIconColorCache(const AppIconColorCache& other) = delete;
  AppIconColorCache& operator=(const AppIconColorCache& other) = delete;
  ~AppIconColorCache() override;

  // Returns a reference to the `AppIconColorCache` instance associated with
  // the given `profile`. Creates a new instance if one does not exist.
  static AppIconColorCache& GetInstance(Profile* profile);

  // Returns the cached light vibrant color of the app icon specified
  // by `app_id`, or calculates and caches the color if it is not cached yet.
  SkColor GetLightVibrantColorForApp(const std::string& app_id,
                                     const gfx::ImageSkia& icon);

  // Returns the cached color of the app icon specified by `app_id`, or
  // calculates and caches the color if it is not cached yet. The returned
  // color can be used to sort icons.
  ash::IconColor GetIconColorForApp(const std::string& app_id,
                                    const gfx::ImageSkia& icon);

  // Removes the color data of the app icon specified by `app_id`.
  void RemoveColorDataForApp(const std::string& app_id);

  // For testing only.
  static std::string EncodeColorForTesting(SkColor color);
  static std::optional<SkColor> DecodeColorForTesting(
      const std::string& string);

 private:
  friend class base::NoDestructor<AppIconColorCache>;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Returns the icon color associated with the given `app_id` from the prefs
  // cache. Returns `std::nullopt` if the target color cannot be found.
  std::optional<IconColor> GetIconColorForAppFromPrefsCache(
      const std::string& app_id);

  // Stores `icon_color` associated with `app_id` in the prefs cache if
  // `profile_` exists.
  void MaybeStoreIconColorToPrefsCache(const std::string& app_id,
                                       const IconColor& icon_color);

  // Returns true when user profile prefs are available for color cache data
  // backup. NOTE: The prefs cache is available only when the persistent icon
  // color cache feature is enabled.
  bool HasPrefsCache() const;

  // `profile_` is nullptr when ~AppIconColorCache() does not need to
  // unsubscribe from it.
  raw_ptr<Profile> profile_;

  // Maps light vibrant colors by app IDs. We maintain this data structure so
  // that we don't have to always resort to the prefs cache. Updates when:
  // 1. A vibrant color is queried for and it is not among these mappings yet;
  // OR
  // 2. Color data of an app is removed.
  using AppIdLightVibrantColor = std::map<std::string, SkColor>;
  AppIdLightVibrantColor vibrant_colors_by_ids_;

  // Maps icon colors by app IDs. We maintain this data structure so
  // that we don't have to always resort to the prefs cache. Updates when:
  // 1. An icon color is queried for and it is not among these mappings yet; OR
  // 2. Color data of an app is removed.
  using AppIdIconColor = std::map<std::string, IconColor>;
  AppIdIconColor icon_colors_by_ids_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_APP_ICON_COLOR_CACHE_APP_ICON_COLOR_CACHE_H_
