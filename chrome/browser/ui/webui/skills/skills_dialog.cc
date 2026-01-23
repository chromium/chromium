// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_dialog.h"

#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"

using content::WebContents;

namespace {

// Default width/height of the Skills dialog.
constexpr gfx::Size kDefaultSize{500, 628};

}  // namespace

namespace skills {

void SkillsDialog::CreateAndShow(tabs::TabInterface* tab) {
  if (!tab || !tab->GetContents()) {
    return;
  }
  if (auto* window = tab->GetBrowserWindowInterface()) {
    Profile* profile = window->GetProfile();
    ShowConstrainedWebDialog(
        profile, std::unique_ptr<SkillsDialog>(new SkillsDialog(profile)),
        tab->GetContents());
  }
  return;
}

SkillsDialog::SkillsDialog(Profile* profile)
    : profile_keep_alive_(profile->GetOriginalProfile(),
                          ProfileKeepAliveOrigin::kSkillsDialog) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  set_can_close(true);
  set_can_resize(false);
  set_can_minimize(true);
  set_dialog_content_url(GURL(std::string(content::kChromeUIScheme) + "://" +
                              chrome::kChromeUISkillsHost + "/dialog"));
  set_dialog_modal_type(ui::mojom::ModalType::kNone);
  set_dialog_size(kDefaultSize);
  // TODO(marissashen): Update to resource once strings are finalized.
  set_dialog_title(u"Skills");
  set_show_dialog_title(true);
  set_delete_on_close(false);
}

SkillsDialog::~SkillsDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

}  // namespace skills
