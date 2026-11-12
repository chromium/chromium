// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SESSION_SESSION_UTIL_H_
#define CHROME_BROWSER_UI_ASH_SESSION_SESSION_UTIL_H_

#include "ui/gfx/image/image_skia.h"

namespace content {
class BrowserContext;
}

namespace user_manager {
class User;
}

gfx::ImageSkia GetAvatarImageForContext(content::BrowserContext* context);
gfx::ImageSkia GetAvatarImageForUser(const user_manager::User* user);

#endif  // CHROME_BROWSER_UI_ASH_SESSION_SESSION_UTIL_H_
