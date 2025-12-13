// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

BrowserWindowCreateParams::BrowserWindowCreateParams(
    BrowserWindowInterface::Type type,
    Profile& profile,
    bool from_user_gesture)
    : type(type), from_user_gesture(from_user_gesture), profile(profile) {}

BrowserWindowCreateParams::BrowserWindowCreateParams(Profile& profile,
                                                     bool from_user_gesture)
    : BrowserWindowCreateParams(BrowserWindowInterface::TYPE_NORMAL,
                                profile,
                                from_user_gesture) {}

BrowserWindowCreateParams::BrowserWindowCreateParams(
    BrowserWindowCreateParams&&) = default;

BrowserWindowCreateParams& BrowserWindowCreateParams::operator=(
    BrowserWindowCreateParams&&) = default;

BrowserWindowCreateParams::~BrowserWindowCreateParams() = default;
