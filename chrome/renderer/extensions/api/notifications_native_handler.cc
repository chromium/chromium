// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/notifications_native_handler.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "chrome/common/extensions/api/notifications/notification_style.h"
#include "extensions/renderer/script_context.h"
#include "gin/data_object_builder.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace extensions {

NotificationsNativeHandler::NotificationsNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void NotificationsNativeHandler::AddRoutes() {
  RouteHandlerFunction(
      "GetNotificationImageSizes", "notifications",
      base::BindRepeating(
          &NotificationsNativeHandler::GetNotificationImageSizes,
          base::Unretained(this)));
}

void NotificationsNativeHandler::GetNotificationImageSizes(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  NotificationBitmapSizes bitmap_sizes = GetNotificationBitmapSizes();

  const float scale_factor = ui::GetScaleForMaxSupportedResourceScaleFactor();

  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);

  struct {
    const char* key;
    const raw_ref<const gfx::Size> size;
  } entries[] = {
      {"icon", raw_ref(bitmap_sizes.icon_size)},
      {"image", raw_ref(bitmap_sizes.image_size)},
      {"buttonIcon", raw_ref(bitmap_sizes.button_icon_size)},
      {"appIconMask", raw_ref(bitmap_sizes.app_icon_mask_size)},
  };

  gin::DataObjectBuilder builder(isolate);
  builder.Set("scaleFactor", scale_factor);
  for (const auto& entry : entries) {
    builder.Set(entry.key, gin::DataObjectBuilder(isolate)
                               .Set("width", entry.size->width())
                               .Set("height", entry.size->height())
                               .Build());
  }

  args.GetReturnValue().Set(builder.Build());
}

}  // namespace extensions
