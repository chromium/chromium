// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/android/omnibox_icon_bridge.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/omnibox/browser/jni_headers/OmniboxIconBridge_jni.h"

namespace omnibox {

OmniboxIconBridge::OmniboxIconBridge(
    JNIEnv* env,
    const jni_zero::JavaRef<JOmniboxIconBridge>& java_bridge)
    : java_bridge_(env, java_bridge) {
  observation_.Observe(&GetOmniboxIconSource());
}

OmniboxIconBridge::~OmniboxIconBridge() = default;

void OmniboxIconBridge::OnIconsChanged() {
  java_bridge_->onIconsChanged(jni_zero::AttachCurrentThread());
}

jni_zero::ScopedJavaLocalRef<JOmniboxIconBridge_RenderedIcon>
OmniboxIconBridge::GetIcon(JNIEnv* env,
                           IconResourceIds icon_id,
                           int32_t size_px) {
  std::optional<RenderedOmniboxIcon> icon =
      RenderOmniboxIconForAndroid(icon_id, size_px);
  if (!icon) {
    return nullptr;
  }
  return OmniboxIconBridge_RenderedIconJni::New(env, icon->bitmap,
                                                icon->is_multi_color);
}

std::optional<RenderedOmniboxIcon> RenderOmniboxIconForAndroid(
    IconResourceIds icon_id,
    int size_px) {
  if (size_px <= 0) {
    return std::nullopt;
  }
  OmniboxIconSource& source = GetOmniboxIconSource();
  // Monochrome icons are painted black and tinted by the consumer on the Java
  // side.
  std::optional<SkBitmap> bitmap =
      source.RenderBitmap(icon_id, size_px, SK_ColorBLACK);
  if (!bitmap || bitmap->drawsNothing()) {
    return std::nullopt;
  }
  return RenderedOmniboxIcon{std::move(*bitmap), source.IsMultiColor(icon_id)};
}

static jni_zero::JniUniquePtr<OmniboxIconBridge> JNI_OmniboxIconBridge_Init(
    JNIEnv* env,
    const jni_zero::JavaRef<JOmniboxIconBridge>& bridge) {
  return jni_zero::MakeUnique<OmniboxIconBridge>(env, bridge);
}

}  // namespace omnibox

DEFINE_JNI(OmniboxIconBridge)
