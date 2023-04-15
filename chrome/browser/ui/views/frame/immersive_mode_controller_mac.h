// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_MAC_H_

#include <memory>

class BrowserView;
class ImmersiveModeController;

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeControllerMac(
    const BrowserView* browser_view);

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_MAC_H_
