// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_EVERYWHERE_OMNIBOX_SERVICE_H_
#define CHROME_BROWSER_UI_OMNIBOX_EVERYWHERE_OMNIBOX_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class OmniboxController;
class Profile;

class EverywhereOmniboxService : public KeyedService,
                                 public ui::GlobalAcceleratorListener::Observer,
                                 public views::WidgetObserver,
                                 public WebUIContentsWrapper::Host {
 public:
  explicit EverywhereOmniboxService(Profile* profile);
  EverywhereOmniboxService(const EverywhereOmniboxService&) = delete;
  EverywhereOmniboxService& operator=(const EverywhereOmniboxService&) = delete;
  ~EverywhereOmniboxService() override;

  void TogglePopup();
  void HidePopup();
  bool IsPopupVisible() const;
  bool IsEverywherePopup(content::WebContents* web_contents) const;
  void OpenUrl(const GURL& url,
               WindowOpenDisposition disposition,
               ui::PageTransition transition = ui::PAGE_TRANSITION_LINK);

  // KeyedService:
  void Shutdown() override;

  // ui::GlobalAcceleratorListener::Observer:
  void OnKeyPressed(const ui::Accelerator& accelerator) override;
  void ExecuteCommand(const std::string& accelerator_group_id,
                      const std::string& command_id) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetClosed(views::Widget* widget);

  // WebUIContentsWrapper::Host:
  void CloseUI() override;
  void ShowUI() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

  views::Widget* GetWidgetForTesting() { return widget_.get(); }
  OmniboxController* GetOmniboxControllerForTesting() {
    return controller_.get();
  }

  void SetIsNavigating(bool is_navigating) { is_navigating_ = is_navigating; }
  void SetWasActiveBeforePopup(bool was_active) {
    was_active_before_popup_ = was_active;
  }

 private:
  void CreateAndShowWidget();

  raw_ptr<Profile> profile_;
  std::unique_ptr<OmniboxController> controller_;
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  std::unique_ptr<views::Widget> widget_;

  bool is_navigating_ = false;
  bool was_active_before_popup_ = false;
  bool creating_everywhere_popup_ = false;
  base::TimeTicks last_key_press_time_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::WeakPtrFactory<EverywhereOmniboxService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_EVERYWHERE_OMNIBOX_SERVICE_H_
