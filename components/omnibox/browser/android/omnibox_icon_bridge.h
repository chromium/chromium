// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ANDROID_OMNIBOX_ICON_BRIDGE_H_
#define COMPONENTS_OMNIBOX_BROWSER_ANDROID_OMNIBOX_ICON_BRIDGE_H_

#include <jni.h>
#include <stdint.h>

#include <optional>

#include "base/scoped_observation.h"
#include "components/omnibox/browser/jni_headers/OmniboxIconBridge_shared_jni.h"
#include "components/omnibox/browser/omnibox_icon_source.h"
#include "third_party/jni_zero/jni_zero.h"
#include "third_party/omnibox_proto/icon_resource_ids.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace omnibox {

// Native counterpart of an OmniboxIconBridge.java instance, which owns it.
// Forwards icon set changes of the current source to the Java bridge.
class OmniboxIconBridge : public OmniboxIconSource::Observer {
 public:
  OmniboxIconBridge(JNIEnv* env,
                    const jni_zero::JavaRef<JOmniboxIconBridge>& java_bridge);
  OmniboxIconBridge(const OmniboxIconBridge&) = delete;
  OmniboxIconBridge& operator=(const OmniboxIconBridge&) = delete;
  ~OmniboxIconBridge() override;

  // OmniboxIconSource::Observer:
  void OnIconsChanged() override;

  // Returns `icon_id` rendered by RenderOmniboxIconForAndroid(), or null.
  jni_zero::ScopedJavaLocalRef<JOmniboxIconBridge_RenderedIcon>
  GetIcon(JNIEnv* env, IconResourceIds icon_id, int32_t size_px);

 private:
  jni_zero::ScopedJavaGlobalRef<JOmniboxIconBridge> java_bridge_;
  base::ScopedObservation<OmniboxIconSource, OmniboxIconSource::Observer>
      observation_{this};
};

struct RenderedOmniboxIcon {
  SkBitmap bitmap;
  // Whether the icon keeps its own colors and must not be tinted.
  bool is_multi_color = false;
};

// Renders `icon_id` from GetOmniboxIconSource() as a `size_px` x `size_px`
// bitmap. Monochrome icons are painted black so the Java side can tint them.
// Returns nullopt if `size_px` is invalid or the icon is unknown. Must be
// called on the UI sequence.
std::optional<RenderedOmniboxIcon> RenderOmniboxIconForAndroid(
    IconResourceIds icon_id,
    int size_px);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_ANDROID_OMNIBOX_ICON_BRIDGE_H_
