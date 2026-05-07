// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
#define COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace send_tab_to_self {

// If this feature is enabled, the notification shown to users will disapear
// after a fixed timeout. When disabled, instead it will remain until the
// user interacts with it either by dismissing or openning it.
BASE_DECLARE_FEATURE(kSendTabToSelfEnableNotificationTimeOut);

// If this feature is enabled, form fields will be extracted from the outgoing
// tab and propagated to the target device.
BASE_DECLARE_FEATURE(kSendTabToSelfPropagateFormFields);

// If this feature is enabled, the scroll position will be captured and
// propagated to the target device using a text fragment selector.
BASE_DECLARE_FEATURE(kSendTabToSelfPropagateScrollPosition);

// If this feature is enabled, more granular "last active" timestamps will be
// shown in the device picker.
BASE_DECLARE_FEATURE(kSendTabToSelfImprovedLastActiveLabels);

// If this feature is enabled, the back/forward history of the shared tab will
// be captured and propagated to the target device.
BASE_DECLARE_FEATURE(kSendTabToSelfPropagateNavigationHistory);

// If this feature is enabled, received tabs will be automatically opened
// in the foreground if Chrome is currently being used.
BASE_DECLARE_FEATURE(kSendTabToSelfAutoOpen);

// If this feature is enabled, several UIs on desktop are enhanced, like showing
// a list of devices directly in the context menu, the new Desktop device picker
// flow, and the visually enhanced STTS target device picker bubble.
BASE_DECLARE_FEATURE(kSendTabToSelfEnhancedDesktopUI);

// If this feature is enabled, a toast will be shown after a tab is successfully
// sent.
BASE_DECLARE_FEATURE(kSendTabToSelfPostSendToast);

// If this feature is enabled, "Send to your devices" entry points will be added
// to the Omnibox context menu.
BASE_DECLARE_FEATURE(kSendTabToSelfExtraEntryPoints);

#if BUILDFLAG(IS_ANDROID)
// If this feature is enabled, physical double-tap gestures will be detected
// and used to trigger sending tabs to self on Android devices.
BASE_DECLARE_FEATURE(kSendTabToSelfGesture);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
// If this feature is enabled, users can schedule tab reminder iOS push
// notifications.
BASE_DECLARE_FEATURE(kIOSTabReminders);

// Convenience method for determining when `kIOSTabReminders` is enabled.
bool AreIOSTabRemindersEnabled();

// Parameter representing the default time offset initially presented in the
// 'Set a Reminder' UI half-sheet. Users can select a different offset manually.
extern const char kReminderNotificationsDefaultTimeOffset[];

// Returns the default time offset used to pre-populate the date/time picker
// when the 'Set a Reminder' UI half-sheet is first shown. This value is
// controlled by the `kReminderNotificationsDefaultTimeOffset` Finch parameter.
const base::TimeDelta GetReminderNotificationsDefaultTimeOffset();
#endif  // BUILDFLAG(IS_IOS)

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
