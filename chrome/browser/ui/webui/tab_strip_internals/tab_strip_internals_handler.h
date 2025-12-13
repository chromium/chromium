// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;
class TabStripInternalsObserver;

// Browser side handler for requests from `chrome://tab-strip-internals` WebUI.
class TabStripInternalsPageHandler
    : public tab_strip_internals::mojom::PageHandler {
 public:
  TabStripInternalsPageHandler(
      Profile* profile,
      mojo::PendingReceiver<tab_strip_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<tab_strip_internals::mojom::Page> page);

  TabStripInternalsPageHandler(const TabStripInternalsPageHandler&) = delete;
  TabStripInternalsPageHandler& operator=(const TabStripInternalsPageHandler&) =
      delete;
  ~TabStripInternalsPageHandler() override;

  // Fetch the current state of all tabstrip models.
  void GetTabStripData(GetTabStripDataCallback callback) override;

 private:
  // Build a snapshot of the current state of all tabstrip models.
  tab_strip_internals::mojom::ContainerPtr BuildSnapshot();
  // Push live updates to the webui.
  void NotifyTabStripUpdated();

  mojo::Receiver<tab_strip_internals::mojom::PageHandler> receiver_;
  mojo::Remote<tab_strip_internals::mojom::Page> page_;

  raw_ptr<Profile> profile_;
  std::unique_ptr<TabStripInternalsObserver> observer_;

  base::WeakPtrFactory<TabStripInternalsPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_HANDLER_H_
