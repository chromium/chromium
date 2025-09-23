// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

class MetricsReporter;
class OmniboxController;
class Profile;

namespace content {
class WebContents;
}  // namespace content

// An observer interface for changes to the WebUI Omnibox popup.
class OmniboxWebuiPopupChangeObserver : public base::CheckedObserver {
 public:
  // Called when a ResizeObserver detects the popup element changed size.
  virtual void OnPopupElementSizeChanged(gfx::Size size) = 0;
};

// Handles bidirectional communication between NTP realbox JS and the browser.
class WebuiOmniboxHandler : public SearchboxHandler {
 public:
  WebuiOmniboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      MetricsReporter* metrics_reporter,
      OmniboxController* omnibox_controller);

  WebuiOmniboxHandler(const WebuiOmniboxHandler&) = delete;
  WebuiOmniboxHandler& operator=(const WebuiOmniboxHandler&) = delete;

  ~WebuiOmniboxHandler() override;

  // Handle observers to be notified of WebUI changes.
  void AddObserver(OmniboxWebuiPopupChangeObserver* observer);
  void RemoveObserver(OmniboxWebuiPopupChangeObserver* observer);
  bool HasObserver(const OmniboxWebuiPopupChangeObserver* observer) const;

  void PopupElementSizeChanged(const gfx::Size& size) override;
  void OnThumbnailRemoved() override {}

  void UpdateSelection(OmniboxPopupSelection old_selection,
                       OmniboxPopupSelection selection);

 private:
  base::ObserverList<OmniboxWebuiPopupChangeObserver> observers_;

  // Size of the WebUI popup element, as reported by ResizeObserver.
  gfx::Size webui_size_;

  base::WeakPtrFactory<WebuiOmniboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_
