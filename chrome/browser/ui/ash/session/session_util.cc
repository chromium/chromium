// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session/session_util.h"

#include "chrome/grit/theme_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"

gfx::ImageSkia GetAvatarImageForContext(content::BrowserContext* context) {
  return GetAvatarImageForUser(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(context));
}

gfx::ImageSkia GetAvatarImageForUser(const user_manager::User* user) {
  static const gfx::ImageSkia* holder =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AVATAR_HOLDER);
  static const gfx::ImageSkia* holder_mask =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_AVATAR_HOLDER_MASK);

  gfx::ImageSkia user_image = user->GetImage();
  gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
      user_image, skia::ImageOperations::RESIZE_BEST, holder->size());
  gfx::ImageSkia masked =
      gfx::ImageSkiaOperations::CreateMaskedImage(resized, *holder_mask);
  return gfx::ImageSkiaOperations::CreateSuperimposedImage(*holder, masked);
}
