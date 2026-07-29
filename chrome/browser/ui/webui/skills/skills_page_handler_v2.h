// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_V2_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_V2_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

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

using ToastType = ::skills::mojom::ToastType;

class SkillsPageHandlerV2 : public ::skills::mojom::SkillsPageHandler {
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
  void SyncCookies(SyncCookiesCallback callback) override;
  void ShowToast(ToastType toast_type) override;
  void InvokeSkill(const std::string& skill_id,
                   const std::string& skill_name,
                   const std::string& skill_icon) override;
  void CloseDialog() override;

 private:
  mojo::Receiver<::skills::mojom::SkillsPageHandler> receiver_;
  const base::raw_ref<Profile> profile_;
  const base::raw_ref<content::WebContents> web_contents_;
  std::unique_ptr<glic::GlicCookieSynchronizer> cookie_synchronizer_;
  base::WeakPtr<SkillsDialogDelegate> delegate_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_V2_H_
