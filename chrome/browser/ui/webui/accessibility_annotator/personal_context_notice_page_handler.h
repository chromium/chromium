// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/accessibility_annotator/personal_context_notice.mojom.h"
#include "chrome/browser/ui/webui/accessibility_annotator/personal_context_notice_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebContents;
}

namespace personal_context::notice {

class PersonalContextNoticePageHandler
    : public personal_context::notice::mojom::PageHandler {
 public:
  PersonalContextNoticePageHandler(
      mojo::PendingReceiver<personal_context::notice::mojom::PageHandler>
          receiver,
      base::OnceCallback<void(NoticeDialogResult)> callback,
      PersonalContextNoticeUI& info_ui,
      content::WebContents* web_contents);
  PersonalContextNoticePageHandler(const PersonalContextNoticePageHandler&) =
      delete;
  PersonalContextNoticePageHandler& operator=(
      const PersonalContextNoticePageHandler&) = delete;
  ~PersonalContextNoticePageHandler() override;

  // personal_context::notice::mojom::PageHandler:
  void GetAccountInfo(GetAccountInfoCallback callback) override;
  void OnInfoAcknowledged() override;
  void OnInfoDismissed() override;
  void OnManageSettingsClicked() override;
  void OnLearnMoreClicked() override;
  void ShowUi() override;

 private:
  mojo::Receiver<personal_context::notice::mojom::PageHandler> receiver_;
  base::OnceCallback<void(NoticeDialogResult)> callback_;
  const raw_ref<PersonalContextNoticeUI> info_ui_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace personal_context::notice

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_PAGE_HANDLER_H_
