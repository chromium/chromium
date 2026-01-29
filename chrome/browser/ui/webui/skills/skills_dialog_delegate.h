// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_DELEGATE_H_

#include <string>

namespace skills {

// A delegate responsible for handling lifecycle and interaction events from the
// Skills dialog.
class SkillsDialogDelegate {
 public:
  // Closes the dialog if it is currently open.
  virtual void CloseDialog() = 0;

  // Called by the WebUI when a skill is successfully saved.
  virtual void OnSkillSaved(const std::string& skill_id) = 0;

 protected:
  virtual ~SkillsDialogDelegate() = default;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_DELEGATE_H_
