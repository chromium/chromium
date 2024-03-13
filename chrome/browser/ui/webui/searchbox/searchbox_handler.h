// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_HANDLER_H_

#include "components/omnibox/browser/omnibox.mojom.h"

// Base class for browser-side handlers that handle bi-directional communication
// with WebUI search boxes.
class SearchboxHandler : public omnibox::mojom::PageHandler {
 protected:
  SearchboxHandler();

  ~SearchboxHandler() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_HANDLER_H_
