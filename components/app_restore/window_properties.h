// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_WINDOW_PROPERTIES_H_
#define COMPONENTS_APP_RESTORE_WINDOW_PROPERTIES_H_

#include "base/component_export.h"
#include "ui/base/class_property.h"

namespace app_restore {

struct WindowInfo;

// Alphabetical sort.

// A property key to store the activation index of an app. Used by ash to
// determine where to stack a window among its siblings. Also used to determine
// if a window is restored by the full restore process. Only a window, restored
// from the full restore file and read by FullRestoreReadHandler during the
// system startup phase, could have a kActivationIndexKey. A smaller index
// indicates a more recently used window. If this key is null, then the window
// was not launched from full restore.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<int32_t*>* const kActivationIndexKey;

// A property key to store the app id.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<std::string*>* const kAppIdKey;

// A property key to indicate that the browser window type is an app type.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<bool>* const kAppTypeBrowser;

// A property key to store the browser app name.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<std::string*>* const kBrowserAppNameKey;

// A property key to indicate the session id for the ARC ghost window from
// RestoreData.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<int32_t>* const kGhostWindowSessionIdKey;

// A property key to store the window id for a Lacros window.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<std::string*>* const kLacrosWindowId;

// A property key indicating whether a window was launched from app restore.
// These windows will not be activatable until they are shown.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<bool>* const kLaunchedFromAppRestoreKey;

// A property key to add the window to a hidden container, if the ARC task is
// not created when the window is initialized.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<bool>* const kParentToHiddenContainerKey;

// A property key indicating whether a ARC ghost window has replaced by real
// ARC task window.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<bool>* const kRealArcTaskWindow;

// A property key to indicate the restore id for the window from RestoreData.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<int32_t>* const kRestoreWindowIdKey;

// A property key to indicate the id for the window to be saved in RestoreData.
// For web apps, browser windows or Chrome app windows, this is the session id.
// For ARC apps, this is the task id.
COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<int32_t>* const kWindowIdKey;

COMPONENT_EXPORT(APP_RESTORE)
extern const ui::ClassProperty<WindowInfo*>* const kWindowInfoKey;

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_WINDOW_PROPERTIES_H_
