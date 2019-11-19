// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_context_util.h"

#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/point.h"
#include "ui/snapshot/snapshot.h"

namespace {

void CreateAssistantStructureAndRunCallback(
    RequestAssistantStructureCallback callback,
    ax::mojom::AssistantExtraPtr assistant_extra,
    const ui::AXTreeUpdate& update) {
  std::move(callback).Run(std::move(assistant_extra),
                          ui::CreateAssistantTree(update, false));
}

ax::mojom::AssistantExtraPtr CreateAssistantExtra(
    content::WebContents* web_contents,
    const gfx::Rect& bounds_pixel) {
  auto assistant_extra = ax::mojom::AssistantExtra::New();
  assistant_extra->url = web_contents->GetLastCommittedURL();
  assistant_extra->title = web_contents->GetTitle();
  assistant_extra->bounds_pixel = bounds_pixel;
  return assistant_extra;
}

}  // namespace

void RequestAssistantStructureForActiveBrowserWindow(
    RequestAssistantStructureCallback callback) {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser || !browser->window()->IsActive()) {
    DCHECK(arc::ArcServiceManager::Get());
    arc::mojom::AppInstance* app_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
        RequestAssistStructure);
    if (!app_instance) {
      std::move(callback).Run(nullptr, nullptr);
      return;
    }

    app_instance->RequestAssistStructure(std::move(callback));
    return;
  }

  // Only returns context from the profile with assistant, which is primary
  // profile.
  if (!chromeos::ProfileHelper::IsPrimaryProfile(browser->profile())) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  aura::Window* window = browser->window()->GetNativeWindow();
  // Ignore incognito window.
  if (window->GetProperty(ash::kBlockedForAssistantSnapshotKey)) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  // We follow same convention as Clank and thus the contents are all in
  // pixels. The bounds of the window need to be converted to pixel in order
  // to be consistent with rest of the view hierarchy.
  gfx::Rect bounds = browser->window()->GetBounds();
  gfx::Point top_left = bounds.origin();
  gfx::Point bottom_right = bounds.bottom_right();
  auto* window_tree_host = window->GetRootWindow()->GetHost();
  // TODO: Revisit once multi-monitor support is planned.
  window_tree_host->ConvertDIPToScreenInPixels(&top_left);
  window_tree_host->ConvertDIPToScreenInPixels(&bottom_right);

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(
          &CreateAssistantStructureAndRunCallback, std::move(callback),
          CreateAssistantExtra(web_contents,
                               gfx::Rect(top_left.x(), top_left.y(),
                                         bottom_right.x() - top_left.x(),
                                         bottom_right.y() - top_left.y()))),
      ui::kAXModeComplete);
}

void RequestAssistantStructureForWebContentsForTesting(
    content::WebContents* web_contents,
    RequestAssistantStructureCallback callback) {
  web_contents->RequestAXTreeSnapshot(
      base::BindOnce(
          &CreateAssistantStructureAndRunCallback, std::move(callback),
          CreateAssistantExtra(web_contents, gfx::Rect(0, 0, 100, 100))),
      ui::kAXModeComplete);
}
