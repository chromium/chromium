// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_V2_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_V2_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "components/skills/public/skills_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;
class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace glic {
class GlicCookieSynchronizer;
}  // namespace glic

namespace signin {
class IdentityManager;
}  // namespace signin

namespace skills {

class SkillsDialogDelegate;

class SkillsPageHandlerV2 : public ::skills::mojom::SkillsPageHandler,
                            public SkillsService::Observer {
 public:
  SkillsPageHandlerV2(
      mojo::PendingReceiver<::skills::mojom::SkillsPageHandler> receiver,
      Profile* profile,
      signin::IdentityManager* identity_manager,
      content::WebContents* web_contents,
      base::WeakPtr<SkillsDialogDelegate> delegate = nullptr);
  SkillsPageHandlerV2(const SkillsPageHandlerV2&) = delete;
  SkillsPageHandlerV2& operator=(const SkillsPageHandlerV2&) = delete;
  ~SkillsPageHandlerV2() override;

  // ::skills::mojom::SkillsPageHandler:
  void SetPage(mojo::PendingRemote<skills::mojom::SkillsPageV2> page) override;
  void GetProvidedSkill(const std::string& skill_id,
                        GetProvidedSkillCallback callback) override;
  void GetProvidedSkills(GetProvidedSkillsCallback callback) override;
  void SyncCookies(SyncCookiesCallback callback) override;
  void ShowSaveToast() override;
  void ShowSaveAndInvokeToast(const std::string& skill_id,
                              const std::string& skill_name,
                              const std::string& skill_icon) override;
  void ShowDeleteToast(const std::string& skill_id,
                       ShowDeleteToastCallback callback) override;
  void InvokeSkill(const std::string& skill_id,
                   const std::string& skill_name,
                   const std::string& skill_icon) override;
  void SendPrompt(const std::string& prompt) override;
  void CloseDialog(::skills::mojom::PendingEditorDataPtr data) override;
  void GetPendingEditorData(GetPendingEditorDataCallback callback) override;

  // SkillsService::Observer:
  void OnProvidedSkillsChanged(SkillsProvider* provider) override;
  void OnSkillUpdated(std::string_view skill_id,
                      SkillsService::UpdateSource update_source,
                      bool is_position_changed) override;

 private:
  BrowserWindowInterface* GetBrowserWindow();

  mojo::Receiver<::skills::mojom::SkillsPageHandler> receiver_;
  mojo::Remote<::skills::mojom::SkillsPageV2> page_;
  const base::raw_ref<Profile> profile_;
  const base::raw_ref<content::WebContents> web_contents_;
  std::unique_ptr<glic::GlicCookieSynchronizer> cookie_synchronizer_;
  base::WeakPtr<SkillsDialogDelegate> delegate_;
  base::ScopedObservation<skills::SkillsService,
                          skills::SkillsService::Observer>
      service_observation_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_V2_H_
