// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_INFO_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_INFO_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info.mojom.h"
#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebContents;
}

namespace accessibility_annotator::info {

class AccessibilityAnnotatorInfoPageHandler
    : public accessibility_annotator::info::mojom::PageHandler {
 public:
  AccessibilityAnnotatorInfoPageHandler(
      mojo::PendingReceiver<accessibility_annotator::info::mojom::PageHandler>
          receiver,
      base::OnceCallback<void(InfoDialogResult)> callback,
      AccessibilityAnnotatorInfoUI& info_ui,
      content::WebContents* web_contents);
  AccessibilityAnnotatorInfoPageHandler(
      const AccessibilityAnnotatorInfoPageHandler&) = delete;
  AccessibilityAnnotatorInfoPageHandler& operator=(
      const AccessibilityAnnotatorInfoPageHandler&) = delete;
  ~AccessibilityAnnotatorInfoPageHandler() override;

  // accessibility_annotator::info::mojom::PageHandler:
  void GetAccountInfo(GetAccountInfoCallback callback) override;
  void OnInfoAcknowledged() override;
  void OnInfoDismissed() override;
  void OnManageSettingsClicked() override;
  void OnLearnMoreClicked() override;
  void ShowUi() override;

 private:
  mojo::Receiver<accessibility_annotator::info::mojom::PageHandler> receiver_;
  base::OnceCallback<void(InfoDialogResult)> callback_;
  const raw_ref<AccessibilityAnnotatorInfoUI> info_ui_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace accessibility_annotator::info

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_INFO_PAGE_HANDLER_H_
