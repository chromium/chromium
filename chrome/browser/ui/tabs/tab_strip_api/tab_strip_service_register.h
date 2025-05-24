// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_REGISTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_REGISTER_H_

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

// This is a public interface used to accept pending receivers to
// tabs_api::mojom::TabStripService.
class TabStripServiceRegister {
 public:
  virtual ~TabStripServiceRegister() = default;
  virtual void Accept(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> client) = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TAB_STRIP_SERVICE_REGISTER_H_
