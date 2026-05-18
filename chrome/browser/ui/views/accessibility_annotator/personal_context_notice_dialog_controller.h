// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_DIALOG_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/webui/accessibility_annotator/personal_context_notice_ui.h"

namespace content {
class BrowserContext;
}

namespace views {
class View;
class Widget;
}  // namespace views

namespace personal_context::notice {

class PersonalContextNoticeDialogController
    : public base::SupportsUserData::Data {
 public:
  static const char kUserDataKey[];

  explicit PersonalContextNoticeDialogController(
      content::BrowserContext* browser_context);
  PersonalContextNoticeDialogController(
      const PersonalContextNoticeDialogController&) = delete;
  PersonalContextNoticeDialogController& operator=(
      const PersonalContextNoticeDialogController&) = delete;
  ~PersonalContextNoticeDialogController() override;

  void ShowDialog(content::WebContents* web_contents,
                  base::OnceCallback<void(NoticeDialogResult)> callback);
  void CloseDialog();

  views::Widget* GetWidgetForTesting() { return dialog_widget_.get(); }

 private:
  raw_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<views::Widget> dialog_widget_;

  base::WeakPtrFactory<PersonalContextNoticeDialogController> weak_factory_{
      this};
};

}  // namespace personal_context::notice

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_DIALOG_CONTROLLER_H_
