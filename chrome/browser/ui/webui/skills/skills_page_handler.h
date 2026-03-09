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
const base::TimeDelta kMax1PDownloadTimeout = base::Seconds(5);

struct PendingSave1PRequest {
  PendingSave1PRequest(std::string id,
                       mojom::PageHandler::MaybeSave1PSkillCallback cb);
  ~PendingSave1PRequest();

  PendingSave1PRequest(PendingSave1PRequest&&);
  PendingSave1PRequest& operator=(PendingSave1PRequest&&);

  std::string skill_id;
  mojom::PageHandler::MaybeSave1PSkillCallback callback;
};

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
  void DeleteSkill(const std::string& skill_id) override;
  void MaybeSave1PSkill(const std::string& skill_id,
                        MaybeSave1PSkillCallback callback) override;

  // skills::SkillsService::Observer:
  void OnSkillUpdated(std::string_view skill_id,
                      SkillsService::UpdateSource update_source,
                      bool is_position_changed) override;
  void OnDiscoverySkillsUpdated(
      const SkillsService::SkillsMap* skills_map) override;
  void OnSkillsServiceShuttingDown() override;

  bool Is1PDownloadTimerRunning() const {
    return first_party_download_timer_.IsRunning();
  }

 private:
  // Triggered if a first party skills download was requested but didn't
  // complete within kMax1PDownloadTimeout seconds.
  void On1PDownloadTimeout();
  mojo::Receiver<skills::mojom::PageHandler> receiver_;
  mojo::Remote<skills::mojom::SkillsPage> page_;
  // Used to timeout if the first party skills download doesn't complete within
  // kMax1PDownloadTimeout seconds.
  base::OneShotTimer first_party_download_timer_;
  const raw_ref<content::WebContents> web_contents_;
  // Initialized with the browser_context passed in the constructor.
  const raw_ref<Profile> profile_;
  // Stores the the request for the most recently clicked save skill button.
  std::optional<PendingSave1PRequest> pending_save_1p_request_;

  base::ScopedObservation<skills::SkillsService,
                          skills::SkillsService::Observer>
      service_observation_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_
