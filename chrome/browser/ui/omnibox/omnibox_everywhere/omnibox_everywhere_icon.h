// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_ICON_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_ICON_H_

#include "ui/gfx/image/image_skia.h"

class TemplateURLService;

namespace omnibox_everywhere {

// Returns the `ImageSkia` icon for Omnibox Everywhere based on branding
// buildflags. On branded builds, the Google "G" is returned when
// `template_url_service` indicates that Google is the default search engine,
// and the generic search loupe is returned otherwise (including when
// `template_url_service` is null).
gfx::ImageSkia GetOmniboxEverywhereIcon(
    const TemplateURLService* template_url_service);

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_ICON_H_
