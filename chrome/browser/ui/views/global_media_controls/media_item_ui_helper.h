// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_HELPER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/media_message_center/notification_theme.h"

class MediaItemUIDeviceSelectorDelegate;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace global_media_controls {
class MediaItemUIDeviceSelector;
class MediaItemUIFooter;
}  // namespace global_media_controls

namespace global_media_controls::mojom {
class DeviceService;
}  // namespace global_media_controls::mojom

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace media_router {
class MediaRoute;
}  // namespace media_router

// A set of the pending remote of `DeviceListHost` and the pending receiver of
// `DeviceListClient`.
struct HostAndClientPair {
  HostAndClientPair();
  HostAndClientPair(HostAndClientPair&&);
  HostAndClientPair& operator=(HostAndClientPair&&);
  ~HostAndClientPair();

  mojo::PendingRemote<global_media_controls::mojom::DeviceListHost> host;
  mojo::PendingReceiver<global_media_controls::mojom::DeviceListClient> client;
};

// These helper functions are shared among Chrome OS and other desktop
// platforms.

// Checks if we should show the device selector view (cast device list) under
// the media ui view. Returns false if any of `device_service`,
// `selector_delegate`, `profile`, `item` is null.
bool ShouldShowDeviceSelectorView(
    Profile* profile,
    global_media_controls::mojom::DeviceService* device_service,
    const std::string& item_id,
    const base::WeakPtr<media_message_center::MediaNotificationItem>& item,
    MediaItemUIDeviceSelectorDelegate* selector_delegate);

// Creates and returns a `HostAndClientPair`.
HostAndClientPair CreateHostAndClient(
    Profile* profile,
    const std::string& id,
    const base::WeakPtr<media_message_center::MediaNotificationItem>& item,
    global_media_controls::mojom::DeviceService* device_service);

// Returns the stop casting callback if it's casting.
base::RepeatingClosure GetStopCastingCallback(
    Profile* profile,
    const std::string& id,
    const base::WeakPtr<media_message_center::MediaNotificationItem>& item);

// Returns whether `item` has an associated Remote Playback route.
bool HasRemotePlaybackRoute(
    base::WeakPtr<media_message_center::MediaNotificationItem> item);

// Returns the MediaRoute associated with `item`, if one exists.
std::optional<media_router::MediaRoute> GetSessionRoute(
    const std::string& item_id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    content::BrowserContext* context);

// Returns the MediaItemUIDeviceSelector for a media item or returns a nullptr
// if any of the parameters is invalid.
std::unique_ptr<global_media_controls::MediaItemUIDeviceSelector>
BuildDeviceSelector(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    global_media_controls::mojom::DeviceService* device_service,
    MediaItemUIDeviceSelectorDelegate* selector_delegate,
    Profile* profile,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    bool show_devices = false,
    std::optional<media_message_center::MediaColorTheme> media_color_theme =
        std::nullopt);

// Returns the MediaItemUIFooter for Cast items or Media Session items with
// associated Media Routes.
std::unique_ptr<global_media_controls::MediaItemUIFooter> BuildFooter(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    Profile* profile,
    std::optional<media_message_center::MediaColorTheme> media_color_theme =
        std::nullopt);

// Returns the MediaColorTheme for the updated media UI.
media_message_center::MediaColorTheme GetMediaColorTheme();

// Return the vector icon for display for the given cast device icon type.
const gfx::VectorIcon& GetVectorIcon(
    global_media_controls::mojom::IconType icon);

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_HELPER_H_
