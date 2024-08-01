// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/cast/mock_cast_toolbar_button_controller.h"

#include "components/media_router/browser/media_router_factory.h"

MockCastToolbarButtonController::MockCastToolbarButtonController(
    Profile* profile)
    : CastToolbarButtonController(
          profile,
          media_router::MediaRouterFactory::GetApiForBrowserContext(profile)) {}

MockCastToolbarButtonController::~MockCastToolbarButtonController() = default;
