// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/mock_media_router_action_controller.h"

#include "components/media_router/browser/media_router_factory.h"

MockMediaRouterActionController::MockMediaRouterActionController(
    Profile* profile)
    : MediaRouterActionController(
          profile,
          media_router::MediaRouterFactory::GetApiForBrowserContext(profile)) {}

MockMediaRouterActionController::~MockMediaRouterActionController() = default;
