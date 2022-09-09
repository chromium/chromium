// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_CONSTRAINED_WINDOW_VIEWS_CLIENT_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_CONSTRAINED_WINDOW_VIEWS_CLIENT_H_

#include <memory>

#include "components/constrained_window/constrained_window_views_client.h"

// Creates a ConstrainedWindowViewsClient for the Chrome environment.
std::unique_ptr<constrained_window::ConstrainedWindowViewsClient>
CreateChromeConstrainedWindowViewsClient();

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_CONSTRAINED_WINDOW_VIEWS_CLIENT_H_
