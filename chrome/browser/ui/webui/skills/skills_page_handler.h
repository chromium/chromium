// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "components/skills/public/skills_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

namespace skills {

class SkillsPageHandler : public skills::mojom::PageHandler,
                          public skills::SkillsService::Observer {
 public:
  SkillsPageHandler(mojo::PendingReceiver<skills::mojom::PageHandler> receiver,
                    mojo::PendingRemote<skills::mojom::SkillsPage> page,
                    content::WebContents* web_contents);

  SkillsPageHandler(const SkillsPageHandler&) = delete;
  SkillsPageHandler& operator=(const SkillsPageHandler&) = delete;

  ~SkillsPageHandler() override;

  // skills::mojom::PageHandler:
  void OpenSkillsDialog(mojom::SkillsDialogType dialog_type,
                        const std::optional<skills::Skill>& skill) override;
  void GetInitialUserSkills(GetInitialUserSkillsCallback callback) override;
  void GetInitial1PSkills(GetInitial1PSkillsCallback callback) override;
  void Request1PSkills() override;

  // skills::SkillsService::Observer:
  void OnSkillUpdated(std::string_view skill_id,
                      SkillsService::UpdateSource update_source) override;
  void OnDiscoverySkillsUpdated(
      const SkillsService::SkillsMap* skills_map) override;
  void OnSkillsServiceShuttingDown() override;

 private:
  mojo::Receiver<skills::mojom::PageHandler> receiver_;
  mojo::Remote<skills::mojom::SkillsPage> page_;
  const raw_ref<content::WebContents> web_contents_;
  // Initialized with the browser_context passed in the constructor.
  const raw_ref<Profile> profile_;

  base::ScopedObservation<skills::SkillsService,
                          skills::SkillsService::Observer>
      service_observation_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_
