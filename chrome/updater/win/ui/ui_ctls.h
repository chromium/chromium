// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_UI_CTLS_H_
#define CHROME_UPDATER_WIN_UI_UI_CTLS_H_

#include "chrome/updater/win/ui/progress_wnd.h"

namespace updater::ui {

inline constexpr ProgressWnd::ControlState ProgressWnd::ctls_[] = {
    // The struct values are:
    // is_ignore_entry, is_visible, is_enabled, is_button, is_default
    {
        IDC_PROGRESS,
        {
            {false, true, false, false, false},   // STATE_INIT
            {false, true, false, false, false},   // STATE_CHECKING_FOR_UPDATE
            {false, true, false, false, false},   // STATE_WAITING_TO_DOWNLOAD
            {false, true, false, false, false},   // STATE_DOWNLOADING
            {false, true, false, false, false},   // STATE_WAITING_TO_INSTALL
            {false, true, false, false, false},   // STATE_INSTALLING
            {false, false, false, false, false},  // STATE_PAUSED
            {false, false, false, false, false},  // STATE_COMPLETE_SUCCESS
            {false, false, false, false, false},  // STATE_COMPLETE_ERROR
            {false, false, false, false,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, false, false, false,
             false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, false, false, false, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, false, false},  // STATE_END
        },
    },
    {
        IDC_PAUSE_RESUME_TEXT,
        {
            {false, false, false, false, false},  // STATE_INIT
            {false, false, false, false, false},  // STATE_CHECKING_FOR_UPDATE
            {false, false, false, false, false},  // STATE_WAITING_TO_DOWNLOAD
            {false, false, true, false, false},   // STATE_DOWNLOADING
            {false, false, false, false, false},  // STATE_WAITING_TO_INSTALL
            {false, false, false, false, false},  // STATE_INSTALLING
            {false, false, true, false, false},   // STATE_PAUSED
            {false, false, false, false, false},  // STATE_COMPLETE_SUCCESS
            {false, false, false, false, false},  // STATE_COMPLETE_ERROR
            {false, false, false, false,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, false, false, false,
             false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, false, false, false, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, false, false},  // STATE_END
        },
    },
    {
        IDC_INFO_TEXT,
        {
            {false, false, false, false, false},  // STATE_INIT
            {false, false, false, false, false},  // STATE_CHECKING_FOR_UPDATE
            {false, false, false, false, false},  // STATE_WAITING_TO_DOWNLOAD
            {false, false, true, false, false},   // STATE_DOWNLOADING
            {false, false, false, false, false},  // STATE_WAITING_TO_INSTALL
            {false, false, false, false, false},  // STATE_INSTALLING
            {false, false, false, false, false},  // STATE_PAUSED
            {false, false, false, false, false},  // STATE_COMPLETE_SUCCESS
            {false, false, false, false, false},  // STATE_COMPLETE_ERROR
            {false, false, false, false,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, false, false, false,
             false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, false, false, false, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, false, false},  // STATE_END
        },
    },
    {
        IDC_INSTALLER_STATE_TEXT,
        {
            {false, true, true, false, false},    // STATE_INIT
            {false, true, true, false, false},    // STATE_CHECKING_FOR_UPDATE
            {false, true, true, false, false},    // STATE_WAITING_TO_DOWNLOAD
            {false, true, true, false, false},    // STATE_DOWNLOADING
            {false, true, true, false, false},    // STATE_WAITING_TO_INSTALL
            {false, true, true, false, false},    // STATE_INSTALLING
            {false, true, true, false, false},    // STATE_PAUSED
            {false, false, false, false, false},  // STATE_COMPLETE_SUCCESS
            {false, false, false, false, false},  // STATE_COMPLETE_ERROR
            {false, false, false, false,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, false, false, false,
             false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, false, false, false, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, false, false},  // STATE_END
        },
    },
    {
        IDC_COMPLETE_TEXT,
        {
            {false, false, false, false, false},  // STATE_INIT
            {false, false, false, false, false},  // STATE_CHECKING_FOR_UPDATE
            {false, false, false, false, false},  // STATE_WAITING_TO_DOWNLOAD
            {false, false, false, false, false},  // STATE_DOWNLOADING
            {false, false, false, false, false},  // STATE_WAITING_TO_INSTALL
            {false, false, false, false, false},  // STATE_INSTALLING
            {false, false, false, false, false},  // STATE_PAUSED
            {false, true, true, false, false},    // STATE_COMPLETE_SUCCESS
            {false, false, false, false, false},  // STATE_COMPLETE_ERROR
            {false, true, true, false,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, true, true, false, false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, true, true, false, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, false, false},  // STATE_END
        },
    },
    {
        IDC_ERROR_TEXT,
        {
            {false, false, false, false, false},  // STATE_INIT
            {false, false, false, false, false},  // STATE_CHECKING_FOR_UPDATE
            {false, false, false, false, false},  // STATE_WAITING_TO_DOWNLOAD
            {false, false, false, false, false},  // STATE_DOWNLOADING
            {false, false, false, false, false},  // STATE_WAITING_TO_INSTALL
            {false, false, false, false, false},  // STATE_INSTALLING
            {false, false, false, false, false},  // STATE_PAUSED
            {false, false, false, false, false},  // STATE_COMPLETE_SUCCESS
            {false, true, true, false, false},    // STATE_COMPLETE_ERROR
            {false, false, false, false,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, false, false, false,
             false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, false, false, false, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, false, false},  // STATE_END
        },
    },
    {
        IDC_ERROR_ILLUSTRATION,
        {
            {false, false, false, false, false},  // STATE_INIT
            {false, false, false, false, false},  // STATE_CHECKING_FOR_UPDATE
            {false, false, false, false, false},  // STATE_WAITING_TO_DOWNLOAD
            {false, false, false, false, false},  // STATE_DOWNLOADING
            {false, false, false, false, false},  // STATE_WAITING_TO_INSTALL
            {false, false, false, false, false},  // STATE_INSTALLING
            {false, false, false, false, false},  // STATE_PAUSED
            {false, false, false, false, false},  // STATE_COMPLETE_SUCCESS
            {false, true, true, false, false},    // STATE_COMPLETE_ERROR
            {false, false, false, false,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, false, false, false,
             false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, false, false, false, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, false, false},  // STATE_END
        },
    },
    {
        IDC_GET_HELP,
        {
            {false, false, false, true, false},  // STATE_INIT
            {false, false, false, true, false},  // STATE_CHECKING_FOR_UPDATE
            {false, false, false, true, false},  // STATE_WAITING_TO_DOWNLOAD
            {false, false, false, true, false},  // STATE_DOWNLOADING
            {false, false, false, true, false},  // STATE_WAITING_TO_INSTALL
            {false, false, false, true, false},  // STATE_INSTALLING
            {false, false, false, true, false},  // STATE_PAUSED
            {false, false, false, true, false},  // STATE_COMPLETE_SUCCESS
            {false, true, true, true, false},    // STATE_COMPLETE_ERROR
            {false, false, false, true,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, false, false, true,
             false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, false, false, true, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, true, false},  // STATE_END
        },
    },
    {
        IDC_BUTTON1,
        {
            {false, false, false, true, false},  // STATE_INIT
            {false, false, false, true, false},  // STATE_CHECKING_FOR_UPDATE
            {false, false, false, true, false},  // STATE_WAITING_TO_DOWNLOAD
            {false, false, false, true, false},  // STATE_DOWNLOADING
            {false, false, false, true, false},  // STATE_WAITING_TO_INSTALL
            {false, false, false, true, false},  // STATE_INSTALLING
            {false, false, false, true, false},  // STATE_PAUSED
            {false, false, false, true, false},  // STATE_COMPLETE_SUCCESS
            {false, false, false, true, false},  // STATE_COMPLETE_ERROR
            {false, true, true, true, true},  // STATE_COMPLETE_RESTART_BROWSER
            {false, true, true, true, true},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, true, true, true, true},  // STATE_COMPLETE_REBOOT
            {false, false, false, true, false},  // STATE_END
        },
    },
    {
        IDC_BUTTON2,
        {
            {false, false, false, true, false},  // STATE_INIT
            {false, false, false, true, false},  // STATE_CHECKING_FOR_UPDATE
            {false, false, false, true, false},  // STATE_WAITING_TO_DOWNLOAD
            {false, false, false, true, false},  // STATE_DOWNLOADING
            {false, false, false, true, false},  // STATE_WAITING_TO_INSTALL
            {false, false, false, true, false},  // STATE_INSTALLING
            {false, false, false, true, false},  // STATE_PAUSED
            {false, false, false, true, false},  // STATE_COMPLETE_SUCCESS
            {false, false, false, true, false},  // STATE_COMPLETE_ERROR
            {false, true, true, true, false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, true, true, true, false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, true, true, true, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, true, false},  // STATE_END
        },
    },
    {
        IDC_CLOSE,
        {
            {false, false, false, true, false},  // STATE_INIT
            {false, false, false, true, false},  // STATE_CHECKING_FOR_UPDATE
            {false, false, false, true, false},  // STATE_WAITING_TO_DOWNLOAD
            {false, false, false, true, false},  // STATE_DOWNLOADING
            {false, false, false, true, false},  // STATE_WAITING_TO_INSTALL
            {false, false, false, true, false},  // STATE_INSTALLING
            {false, false, false, true, false},  // STATE_PAUSED
            {false, true, true, true, true},     // STATE_COMPLETE_SUCCESS
            {false, true, true, true, true},     // STATE_COMPLETE_ERROR
            {false, false, false, true,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, false, false, true,
             false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, false, false, true, false},  // STATE_COMPLETE_REBOOT
            {false, false, false, true, false},  // STATE_END
        },
    },
    {
        IDC_APP_BITMAP,
        {
            {false, true, false, false, false},   // STATE_INIT
            {false, true, false, false, false},   // STATE_CHECKING_FOR_UPDATE
            {false, true, false, false, false},   // STATE_WAITING_TO_DOWNLOAD
            {false, true, false, false, false},   // STATE_DOWNLOADING
            {false, true, false, false, false},   // STATE_WAITING_TO_INSTALL
            {false, true, false, false, false},   // STATE_INSTALLING
            {false, true, false, false, false},   // STATE_PAUSED
            {false, true, false, false, false},   // STATE_COMPLETE_SUCCESS
            {false, false, false, false, false},  // STATE_COMPLETE_ERROR
            {false, true, false, false,
             false},  // STATE_COMPLETE_RESTART_BROWSER
            {false, true, false, false,
             false},  // COMPLETE_RESTART_ALL_BROWSERS
            {false, true, false, false, false},  // STATE_COMPLETE_REBOOT
            {false, true, false, false, false},  // STATE_END
        },
    },
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_UI_CTLS_H_
