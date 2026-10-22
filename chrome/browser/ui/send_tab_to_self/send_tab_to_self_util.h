// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace send_tab_to_self {

class SendTabToSelfEntry;
enum class SendTabToSelfResult;
enum class ShareActivatedEntryPoint;

}  // namespace send_tab_to_self

class Profile;

namespace send_tab_to_self {

// Marks the STTS entry matching `guid` as activated in the model.
void MarkEntryMatchingGuidActivated(Profile* profile,
                                    const std::string& guid,
                                    ShareActivatedEntryPoint entry_point);

// Opens the given `entry` in a new foreground tab for the given `profile`.
// Returns a weak pointer to the opened WebContents.
base::WeakPtr<content::WebContents> OpenEntryInNewForegroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry,
    ShareActivatedEntryPoint entry_point);

// Opens the given `entry` in a new background tab for the given `profile`.
// Returns a weak pointer to the opened WebContents.
base::WeakPtr<content::WebContents> OpenEntryInNewBackgroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry);

// Returns the vector icon corresponding to `form_factor`.
const gfx::VectorIcon& GetDeviceIcon(
    syncer::DeviceInfo::FormFactor form_factor);

// Shows the appropriate toast in `web_contents` for `result` after attempting
// to send a tab to a target device. Does nothing if `web_contents` is null.
void OnSendTabToDeviceComplete(
    base::WeakPtr<content::WebContents> web_contents,
    const std::string& device_name,
    syncer::DeviceInfo::FormFactor form_factor,
    SendTabToSelfResult result);

// Opens the "Manage account devices" page for `profile` with `disposition`.
void OpenManageDevicesPage(Profile* profile,
                           WindowOpenDisposition disposition =
                               WindowOpenDisposition::kNewForegroundTab);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
