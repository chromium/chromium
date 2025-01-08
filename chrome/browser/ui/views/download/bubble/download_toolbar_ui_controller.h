// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_UI_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/download/download_display.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace offline_items_collection {
struct ContentId;
}

class Browser;
class BrowserView;
class DownloadDisplayController;
class DownloadBubbleUIController;

// DownloadToolbarUIController is a controller for the downloads button shown in
// the trusted area of the toolbar. This controller manages state, animations,
// and badges for the button. The icon is made visible when pinned or when
// downloads are in progress or when a download was initiated in the past 1
// hour.
class DownloadToolbarUIController : public DownloadDisplay,
                                    public BrowserListObserver {
 public:
  explicit DownloadToolbarUIController(BrowserView* browser_view);
  DownloadToolbarUIController(const DownloadToolbarUIController&) = delete;
  DownloadToolbarUIController& operator=(const DownloadToolbarUIController&) =
      delete;
  ~DownloadToolbarUIController() override;

  // DownloadDisplay:
  void Show() override;
  void Hide() override;
  bool IsShowing() const override;
  void Enable() override;
  void Disable() override;
  void UpdateDownloadIcon(const IconUpdateInfo& updates) override;
  void ShowDetails() override;
  void HideDetails() override;
  bool IsShowingDetails() const override;
  void AnnounceAccessibleAlertNow(const std::u16string& alert_text) override;
  bool IsFullscreenWithParentViewHidden() const override;
  bool ShouldShowExclusiveAccessBubble() const override;
  void OpenSecuritySubpage(
      const offline_items_collection::ContentId& id) override;
  IconState GetIconState() const override;

  void UpdateIcon();

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  void InvokeUI();

  DownloadBubbleUIController* bubble_controller() {
    return bubble_controller_.get();
  }

  DownloadDisplayController* display_controller() { return controller_.get(); }

 private:
  void CloseAutofillPopup();

  // Makes the required visual changes to set/unset the button into a dormant
  // or normal state.
  void UpdateIconDormant();

  raw_ptr<BrowserView> browser_view_;
  base::WeakPtr<actions::ActionItem> action_item_ = nullptr;
  // Controller for the DownloadToolbarButton UI.
  std::unique_ptr<DownloadDisplayController> controller_;
  // Controller for keeping track of items for both main view and partial view.
  std::unique_ptr<DownloadBubbleUIController> bubble_controller_;

  // Whether the progress ring in the icon should be updated continuously
  // (false), or the icon should be displayed as dormant (true). This is a
  // performance optimization to avoid redrawing the progress ring too many
  // times when a download is occurring. If the button is dormant, the progress
  // ring is drawn as a solid circle, and the icon color is the inactive color.
  // The badge is still drawn, and the icon shape is still updated. (These
  // change relatively infrequently, so it's ok to update them when they
  // change.) Buttons on browsers other than the most recent active browser for
  // the profile are generally dormant.
  bool is_dormant_ = false;

  // Following 2 fields are updated by the display controller and determine the
  // visual characteristics of the button icon. Note that they are still updated
  // as if the button is normal, even when the button is in a dormant state.

  // Current or pending state of the icon. If changing these, trigger
  // UpdateIcon() afterwards.
  IconState state_ = IconState::kComplete;
  IconActive active_ = IconActive::kInactive;

  // Maps number of in-progress downloads to the corresponding tooltip text, to
  // avoid having to create the strings repeatedly. The entry for 0 is the
  // default tooltip ("Downloads"), the entries for larger numbers are the
  // tooltips for N in-progress downloads ("N downloads in progress").
  std::map<int, std::u16string> tooltip_texts_;

  base::WeakPtrFactory<DownloadToolbarUIController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_TOOLBAR_UI_CONTROLLER_H_
