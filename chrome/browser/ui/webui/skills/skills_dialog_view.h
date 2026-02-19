// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

class Profile;

namespace skills {

// The contents view for the Skills dialog. It hosts the WebView that renders
// the Skills WebUI and manages the layout and dimensions of the dialog content.
class SkillsDialogView : public views::View,
                         public content::WebContentsDelegate {
  METADATA_HEADER(SkillsDialogView, views::View)
 public:
  explicit SkillsDialogView(Profile* profile);
  SkillsDialogView(const SkillsDialogView&) = delete;
  SkillsDialogView& operator=(const SkillsDialogView&) = delete;
  ~SkillsDialogView() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSkillsDialogElementId);

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // contents::WebContentsDelegate:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;

  content::WebContents* web_contents() { return web_view_->GetWebContents(); }
  views::WebView* web_view() { return web_view_; }

 private:
  raw_ptr<views::WebView> web_view_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_VIEW_H_
