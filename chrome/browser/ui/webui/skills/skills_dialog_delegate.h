// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_DELEGATE_H_

#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"

class BrowserWindowInterface;

namespace skills {

// A delegate responsible for handling lifecycle and interaction events from the
// Skills dialog.
class SkillsDialogDelegate {
 public:
  // Closes the dialog if it is currently open.
  virtual void CloseDialog() = 0;

  // Called by the WebUI when a skill is successfully saved.
  virtual void OnSkillSaved(const std::string& skill_id) = 0;
  virtual void OnSkillSaved(const std::string& skill_id,
                            const std::string& skill_name,
                            const std::string& skill_icon) = 0;

  // Called by the WebUI when a skill is deleted.
  virtual void OnSkillDeleted(const std::string& skill_id) = 0;
  virtual void OnSkillDeleted(const std::string& skill_id,
                              base::OnceCallback<void(bool)> callback) = 0;

  // Returns the browser window associated with the dialog.
  virtual BrowserWindowInterface* GetBrowserWindowInterface() = 0;

  // Invokes the skill with skill_id in sidepanel.
  virtual void InvokeSkill(std::string_view skill_id) {}
  virtual void InvokeSkill(std::string_view skill_id,
                           std::string_view skill_name,
                           std::string_view skill_icon,
                           bool auto_submit) {}

  // Sends a prompt to the side panel.
  virtual void SendPrompt(std::string_view prompt) {}

 protected:
  virtual ~SkillsDialogDelegate() = default;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_DELEGATE_H_
