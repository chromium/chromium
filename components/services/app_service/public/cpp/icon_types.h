// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_TYPES_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_TYPES_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {

struct COMPONENT_EXPORT(ICON_TYPES) IconKey {
  IconKey();
  IconKey(uint64_t timeline, int32_t resource_id, uint32_t icon_effects);

  IconKey(const IconKey&) = delete;
  IconKey& operator=(const IconKey&) = delete;
  IconKey(IconKey&&) = default;
  IconKey& operator=(IconKey&&) = default;

  ~IconKey();

  bool operator==(const IconKey& other) const;
  bool operator!=(const IconKey& other) const;

  std::unique_ptr<IconKey> Clone() const;

  // A timeline value for icons that do not change.
  static const uint64_t kDoesNotChangeOverTime;

  static const int32_t kInvalidResourceId;

  // A monotonically increasing number so that, after an icon update, a new
  // IconKey, one that is different in terms of field-by-field equality, can be
  // broadcast by a Publisher.
  //
  // The exact value of the number isn't important, only that newer IconKey's
  // (those that were created more recently) have a larger timeline than older
  // IconKey's.
  //
  // This is, in some sense, *a* version number, but the field is not called
  // "version", to avoid any possible confusion that it encodes *the* app's
  // version number, e.g. the "2.3.5" in "FooBar version 2.3.5 is installed".
  //
  // For example, if an app is disabled for some reason (so that its icon is
  // grayed out), this would result in a different timeline even though the
  // app's version is unchanged.
  uint64_t timeline = 0;

  // True when the raw icon is updated. After an icon update, a new IconKey can
  // be broadcast by a Publisher. Then the AppService icon directory should be
  // removed to fetch the new raw icon from the app.
  //
  // When the raw icon is updated, `timeline` should be updated, so we don't
  // need to check `raw_icon_updated` for `operator==` and `operator!=`.
  bool raw_icon_updated = false;

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
struct COMPONENT_EXPORT(ICON_TYPES) IconValue {
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

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_TYPES_H_
