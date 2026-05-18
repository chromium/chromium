// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_DIALOG_H_

#include <memory>

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/accessibility_annotator/personal_context_notice_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace personal_context::notice {

class PersonalContextNoticeDialog : public WebUIBubbleDialogView {
  METADATA_HEADER(PersonalContextNoticeDialog, WebUIBubbleDialogView)

 public:
  PersonalContextNoticeDialog(
      views::View* anchor_view,
      std::unique_ptr<WebUIContentsWrapperT<PersonalContextNoticeUI>>
          contents_wrapper);
  PersonalContextNoticeDialog(const PersonalContextNoticeDialog&) = delete;
  PersonalContextNoticeDialog& operator=(const PersonalContextNoticeDialog&) =
      delete;
  ~PersonalContextNoticeDialog() override;

 private:
  std::unique_ptr<WebUIContentsWrapperT<PersonalContextNoticeUI>>
      contents_wrapper_;
};

}  // namespace personal_context::notice

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_DIALOG_H_
