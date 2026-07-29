// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_ICON_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_ICON_UTILS_H_

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"

#ifdef __OBJC__
#include <map>

@class NSImage;
@class NSImageRep;
#endif

namespace gfx {
class Image;
}

namespace web_app {

// Creates a masked icon image from a base icon image. Without resizing
// `base_icon`, returns an icon masked to a rounded rect according to Apple
// design templates.
gfx::Image CreateAppleMaskedAppIcon(const gfx::Image& base_icon);

// Creates the macOS apps folder image with Chrome Apps overlays on a thread
// pool and returns it asynchronously. Runs callback on the UI thread.
void GetMacAppsFolderImageAsync(int size,
                                base::OnceCallback<void(gfx::Image)> callback);

#ifdef __OBJC__
// Map from resource ID to NSImageRep pointer. Objective-C object pointers are
// automatically ref-counted and safe to pass across threads.
using ResourceIDToImage = std::map<int, NSImageRep*>;

// Generates a map of NSImageReps used for the Mac apps folder icon.
// Must be called on the UI thread as it uses ui::ResourceBundle.
ResourceIDToImage GetImageResourcesOnUIThread();

// Builds the macOS apps folder NSImage using the given resource representations
// map.
NSImage* CreateMacAppsFolderIcon(const ResourceIDToImage& images);

// Check if an icon has a solid color border
bool HasSolidColorBorder(const gfx::Image& icon);

// Creates a masked icon image from a base icon image. This mask is only
// for the DIY app. After resizing, adding a white background, and masking
// the icon, returns an icon masked to a rounded rect according to Apple
// design templates.
gfx::Image MaskDiyAppIcon(const gfx::Image& base_icon);

// Creates a canvas the same size as `overlay`, copies the appropriate
// representation from `background` into it (according to Cocoa), then draws
// `overlay` over it using NSCompositingOperationSourceOver.
NSImageRep* OverlayImageRep(NSImage* background, NSImageRep* overlay);
#endif

namespace testing {
// Sets whether icon masking should be disabled for testing purposes.
[[nodiscard]] base::AutoReset<bool> SetDisableIconMaskingForTesting(
    bool disabled);
}  // namespace testing

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_ICON_UTILS_H_
