// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_INTERNALS_ACCESSIBILITY_ANNOTATOR_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_INTERNALS_ACCESSIBILITY_ANNOTATOR_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/accessibility_annotator_internals/accessibility_annotator_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}

class AccessibilityAnnotatorInternalsPageHandler
    : public browser::accessibility_annotator_internals::mojom::PageHandler {
 public:
  explicit AccessibilityAnnotatorInternalsPageHandler(
      mojo::PendingReceiver<
          browser::accessibility_annotator_internals::mojom::PageHandler>
          receiver,
      Profile* profile,
      content::WebContents* web_contents);
  ~AccessibilityAnnotatorInternalsPageHandler() override;

  AccessibilityAnnotatorInternalsPageHandler(
      const AccessibilityAnnotatorInternalsPageHandler&) = delete;
  AccessibilityAnnotatorInternalsPageHandler& operator=(
      const AccessibilityAnnotatorInternalsPageHandler&) = delete;

  // browser::accessibility_annotator_internals::mojom::PageHandler:
  void TriggerFirstRun(TriggerFirstRunCallback callback) override;

 private:
  mojo::Receiver<browser::accessibility_annotator_internals::mojom::PageHandler>
      receiver_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_INTERNALS_ACCESSIBILITY_ANNOTATOR_INTERNALS_PAGE_HANDLER_H_
