// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file maps resource IDs to Android resource IDs.

// Presence of regular include guards is checked by:
// 1. cpplint
// 2. a custom presubmit in src/PRESUBMIT.py
// 3. clang (but it only checks the guard is correct if present)
// Disable the first two with these magic comments:
// NOLINT(build/header_guard)
// no-include-guard-because-multiply-included

// LINK_RESOURCE_ID is used for IDs that come from a .grd file.
#ifndef LINK_RESOURCE_ID
#error "LINK_RESOURCE_ID should be defined before including this file"
#endif
// DECLARE_RESOURCE_ID is used for IDs that don't have .grd entries, and
// are only declared in this file.
#ifndef DECLARE_RESOURCE_ID
#error "DECLARE_RESOURCE_ID should be defined before including this file"
#endif

// PageInfoUI images, used in ConnectionInfoView
// Good:
DECLARE_RESOURCE_ID(IDR_PAGEINFO_GOOD, R.drawable.pageinfo_good)
DECLARE_RESOURCE_ID(IDR_PAGEINFO_GOOD_V2, R.drawable.omnibox_https_valid)
// Warnings:
DECLARE_RESOURCE_ID(IDR_PAGEINFO_WARNING_MINOR, R.drawable.pageinfo_warning)
// Bad:
DECLARE_RESOURCE_ID(IDR_PAGEINFO_BAD, R.drawable.pageinfo_bad)
DECLARE_RESOURCE_ID(IDR_PAGEINFO_BAD_V2, R.drawable.omnibox_not_secure_warning)
// Should never occur, use warning just in case:
// Enterprise managed: ChromeOS only.
DECLARE_RESOURCE_ID(IDR_PAGEINFO_ENTERPRISE_MANAGED,
                    R.drawable.pageinfo_warning)
// Info: Only shown on chrome:// urls, which don't show the connection info
// popup.
DECLARE_RESOURCE_ID(IDR_PAGEINFO_INFO, R.drawable.pageinfo_warning)
// Major warning: Used on insecure pages, which don't show the connection info
// popup.
DECLARE_RESOURCE_ID(IDR_PAGEINFO_WARNING_MAJOR, R.drawable.pageinfo_warning)

// PageInfoUI colors, used in ConnectionInfoView
// Good:
DECLARE_RESOURCE_ID(IDR_PAGEINFO_GOOD_COLOR, R.color.default_icon_color)
// Warning:
DECLARE_RESOURCE_ID(IDR_PAGEINFO_WARNING_COLOR, R.color.default_icon_color_blue)
// Bad:
DECLARE_RESOURCE_ID(IDR_PAGEINFO_BAD_COLOR, R.color.default_text_color_error)