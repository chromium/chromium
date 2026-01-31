// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_H_

#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Profile;

namespace skills {

class SkillsDialog : public ui::WebDialogDelegate {
 public:
  explicit SkillsDialog(Profile* profile);
  SkillsDialog(const SkillsDialog&) = delete;
  SkillsDialog& operator=(const SkillsDialog&) = delete;
  ~SkillsDialog() override;

 private:
  // Prevent Profile destruction until the dialog is closed, to prevent a
  // dangling RenderProcessHost crash.
  ScopedProfileKeepAlive profile_keep_alive_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_H_
