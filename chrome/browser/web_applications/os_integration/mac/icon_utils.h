// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_ICON_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_ICON_UTILS_H_

@class NSImage;
@class NSImageRep;
namespace gfx {
class Image;
}

namespace web_app {

// Creates a masked icon image from a base icon image. Without resizing
// `base_icon`, returns an icon masked to a rounded rect according to Apple
// design templates.
gfx::Image CreateAppleMaskedAppIcon(const gfx::Image& base_icon);

// Creates a canvas the same size as `overlay`, copies the appropriate
// representation from `background` into it (according to Cocoa), then draws
// `overlay` over it using NSCompositingOperationSourceOver.
NSImageRep* OverlayImageRep(NSImage* background, NSImageRep* overlay);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_ICON_UTILS_H_
