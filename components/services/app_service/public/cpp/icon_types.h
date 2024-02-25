// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_TYPES_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_TYPES_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

struct COMPONENT_EXPORT(APP_TYPES) IconKey {
  // The value of a UpdateVersion can be a bool or a int32_t, depending on
  // whether the IconKey is being passed between a publisher and App Service, or
  // App Service and clients.
  using UpdateVersion = absl::variant<bool, int32_t>;

  IconKey();
  explicit IconKey(uint32_t icon_effects);
  IconKey(int32_t resource_id, uint32_t icon_effects);
  IconKey(bool raw_icon_updated, uint32_t icon_effects);

  IconKey(const IconKey&) = delete;
  IconKey& operator=(const IconKey&) = delete;
  IconKey(IconKey&&) = default;
  IconKey& operator=(IconKey&&) = default;

  ~IconKey();

  bool operator==(const IconKey& other) const;
  bool operator!=(const IconKey& other) const;

  std::unique_ptr<IconKey> Clone() const;

  // Returns true when `update_version` is set as true. This is used for
  // publishers to update the icon image per the app's request.
  bool HasUpdatedVersion() const;

  static const int32_t kInvalidResourceId;

  static const int32_t kInitVersion;

  static const int32_t kInvalidVersion;

  // `update_version` should hold a bool to specify whether the icon image is
  // updated by the app when the publisher publishes the app. `update_version`
  // should never hold a int32_t when calling OnApps to publish apps.
  //
  // AppRegistryCache generates an int32_t for `update_version` to notify
  // AppService clients whether reload the icon, when merging `deltas`.
  // `update_version` should never hold a bool in AppRegistryCache's `states_`.
  //
  // If the icon has a valid `resource_id`, `update_version` is
  // `kInvalidVersion` in AppRegistryCache's `states_`, and never changes.
  //
  // When an app is added, if the icon doesn't have a valid `resource_id`,
  // `update_version` is set as `kInitVersion`.
  //
  // If the app updates the icon image, `update_version` is set as true by a
  // Publisher, and AppService increases the saved `update_version` to notify
  // AppService clients to reload icons.
  //
  // The exact value of the number isn't important, only that newer
  // IconKey's (those that were created more recently) have a larger
  // `update_version` than older IconKey's.
  //
  // `update_version` isn't changed if `icon_effects` is changed.
  // `update_version` is increased only when the app updates, or upgrades, and
  // publish a new icon image.
  //
  // The default value is set as false to ease the publishers implementation
  // when calling OnApps.
  UpdateVersion update_version = false;

  // If non-zero (or equivalently, not equal to kInvalidResourceId), the
  // compressed icon is compiled into the Chromium binary as a statically
  // available, int-keyed resource.
  int32_t resource_id = kInvalidResourceId;

  // A bitmask of icon post-processing effects, such as desaturation to gray
  // and rounding the corners.
  uint32_t icon_effects = 0;

  // When adding new fields, also update the IconLoader::Key type in
  // components/services/app_service/public/cpp/icon_loader.*
};

using IconKeyPtr = std::unique_ptr<IconKey>;

enum class IconType {
  // Sentinel value used in error cases.
  kUnknown,
  // Icon as an uncompressed gfx::ImageSkia with no standard Chrome OS mask.
  kUncompressed,
  // Icon as compressed PNG-encoded bytes with no standard Chrome OS mask.
  kCompressed,
  // Icon as an uncompressed gfx::ImageSkia with the standard Chrome OS mask
  // applied. This is the default suggested icon type.
  kStandard,
};

// The return value for the App Service LoadIcon method. The icon will be
// provided in either an uncompressed representation (gfx::ImageSkia), or a
// compressed representation (PNG-encoded bytes) depending on |icon_type|.
struct COMPONENT_EXPORT(APP_TYPES) IconValue {
  IconValue();

  IconValue(const IconValue&) = delete;
  IconValue& operator=(const IconValue&) = delete;

  ~IconValue();

  IconType icon_type = IconType::kUnknown;

  gfx::ImageSkia uncompressed;

  // PNG-encoded bytes for the icon
  std::vector<uint8_t> compressed;

  // Specifies whether the icon provided is a maskable icon. This field should
  // only be true if the icon type is kCompressed, and the compressed icon data
  // is from a maskable icon.
  bool is_maskable_icon = false;

  // PNG-encoded bytes for the foreground icon data. Only available for the
  // adaptive icon, e.g. some ARC app icons, when `icon_type` is kUncompressed.
  // This field should be set by GetCompressedIconData only for
  // publishers to get the raw icon data.
  std::vector<uint8_t> foreground_icon_png_data;

  // PNG-encoded bytes for the background icon data. Only available for the
  // adaptive icon, e.g. some ARC app icons, when `icon_type` is kUncompressed.
  // This field should be set by GetCompressedIconData only for
  // publishers to get the raw icon data.
  std::vector<uint8_t> background_icon_png_data;

  // Specifies whether the icon provided is a placeholder. That field should
  // only be true if the corresponding `LoadIcon` call had
  // `allow_placeholder_icon` set to true, which states whether the caller will
  // accept a placeholder if the real icon can not be provided at this time.
  bool is_placeholder_icon = false;
};

using IconValuePtr = std::unique_ptr<IconValue>;
using LoadIconCallback = base::OnceCallback<void(IconValuePtr)>;

// Merges `delta` to `state`, and  returns's the merge result. If `delta`'s
// `update_version` is true, increase `state`'s `update_version`.
COMPONENT_EXPORT(APP_TYPES)
std::optional<apps::IconKey> MergeIconKey(const apps::IconKey* state,
                                          const apps::IconKey* delta);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_TYPES_H_
