// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_SERVICE_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_SERVICE_H_

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

class Profile;

class OmniboxEverywhereService : public KeyedService,
                                 public ui::GlobalAcceleratorListener::Observer,
                                 public views::WidgetObserver,
                                 public WebUIContentsWrapper::Host {
 public:
  explicit OmniboxEverywhereService(Profile* profile);
  OmniboxEverywhereService(const OmniboxEverywhereService&) = delete;
  OmniboxEverywhereService& operator=(const OmniboxEverywhereService&) = delete;
  ~OmniboxEverywhereService() override;

  void TogglePopup();
  void HidePopup();
  bool IsPopupVisible() const;
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
  void OnWidgetDestroying(views::Widget* widget) override;

  // WebUIContentsWrapper::Host:
  void CloseUI() override;
  void ShowUI() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;

  void OnFileChooserOpened();
  void OnFileChooserClosed();

  base::WeakPtr<OmniboxEverywhereService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  views::Widget* GetWidgetForTesting() { return widget_.get(); }
  bool is_file_chooser_open_for_testing() const {
    return is_file_chooser_open_;
  }

  void SetIsNavigating(bool is_navigating) { is_navigating_ = is_navigating; }
  void SetWasActiveBeforePopup(bool was_active) {
    was_active_before_popup_ = was_active;
  }

 private:
  void CreateAndShowWidget();

  raw_ptr<Profile> profile_;
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  std::unique_ptr<views::Widget> widget_;

  bool is_navigating_ = false;
  bool was_active_before_popup_ = false;
  bool is_file_chooser_open_ = false;
  base::TimeTicks last_key_press_time_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::WeakPtrFactory<OmniboxEverywhereService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_SERVICE_H_
