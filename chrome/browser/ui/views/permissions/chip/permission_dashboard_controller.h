// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_reopen_suppressor.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/view_tracker.h"

class LocationBar;
class ChipController;
class ContentSettingImageModel;
class ContentSettingImageViewDelegate;
class PermissionDashboardInterface;

class PermissionDashboardController : public PermissionChipInterface::Observer {
 public:
  PermissionDashboardController(
      LocationBar* location_bar,
      ContentSettingImageViewDelegate* content_settings_image_delegate,
      PermissionDashboardInterface* permission_dashboard);

  ~PermissionDashboardController() override;
  PermissionDashboardController(const PermissionDashboardController&) = delete;
  PermissionDashboardController& operator=(
      const PermissionDashboardController&) = delete;

  ChipController* request_chip_controller() {
    return request_chip_controller_.get();
  }

  PermissionDashboardInterface* permission_dashboard() {
    return permission_dashboard_;
  }

  // This method updates UI based on `ContentSettingImageModel` state. Returns
  // `true` if there are user-visible changes, otherwise returns `false`.
  //
  // Assumes that `content_settings_image_delegate` passed to the constructor is
  // appropriate to use with `indicator_model`.
  bool Update(ContentSettingImageModel* indicator_model);

  // PermissionChipInterface::Observer
  void OnChipVisibilityChanged(bool is_visible) override;
  void OnExpandAnimationEnded() override;
  void OnCollapseAnimationEnded() override;
  void OnMousePressed() override;

  bool is_verbose() const { return is_verbose_; }

  // Returns `true` if currently visible verbose indicator should be suppressed
  // by e.g. an incoming permission request and `collapse_timer_` will fired if
  // running. Return `false` otherwise.
  bool SuppressVerboseIndicator();

  base::OneShotTimer& get_collapse_timer_for_testing() {
    return collapse_timer_;
  }

  views::View* page_info_for_testing() {
    return page_info_bubble_suppressor_.GetWidget()
               ? page_info_bubble_suppressor_.GetWidget()
                     ->widget_delegate()
                     ->GetContentsView()
               : nullptr;
  }

  void ShowPageInfoDialogForTesting() { ShowPageInfoDialog(true); }

  void DoNotCollapseForTesting();

  void HideIndicatorsForTesting() { HideIndicators(); }

  void SetSuppressionThresholdForTesting(base::TimeDelta threshold) {
    page_info_bubble_suppressor_.SetSuppressionThresholdForTesting(  // IN-TEST
        threshold);
  }

 private:
  void StartCollapseTimer();
  void Collapse(bool hide);
  void HideIndicators();
  void ShowBubble();
  void ShowPageInfoDialog(bool is_pointer_interaction);
  // Actions executed when the user closes the page info dialog.
  void OnIndicatorsChipButtonPressed(bool is_pointer_interaction);
  std::u16string GetIndicatorTitle(ContentSettingImageModel* model);
  std::u16string GetSensorsIndicatorTitle(ContentSettingImageModel* model);
  std::u16string GetMediaStreamIndicatorTitle(ContentSettingImageModel* model);

  // The implementation of `LocationBar` owns this.
  raw_ptr<LocationBar> location_bar_ = nullptr;

  // This image delegate is passed w/the location bar, and this class assumes
  // that it's the appropriate delegate to use for everything shown.
  raw_ptr<ContentSettingImageViewDelegate> content_setting_image_delegate_ =
      nullptr;
  raw_ptr<PermissionDashboardInterface> permission_dashboard_ = nullptr;
  // Currently Camera, Mic and Sensors are supported.
  raw_ptr<ContentSettingImageModel> content_setting_image_model_ = nullptr;
  std::unique_ptr<ChipController> request_chip_controller_;
  // A timer used to collapse indicators after a delay.
  base::OneShotTimer collapse_timer_;
  bool do_no_collapse_for_testing_ = false;
  // A flag that reflects a visual condition of the LHS indicator chip.
  // `true` - is used for a verbose state that includes an icon + text. Its
  // appearance is accompanied by an expand and collapse animation.
  // `false` - is used for a collapsed (not verbose) state that includes only an
  // icon. It appears without animation.
  bool is_verbose_ = false;
  bool blocked_on_system_level_ = false;
  content::GlobalRenderFrameHostId main_frame_id_;
  WebUIBubbleReopenSuppressor page_info_bubble_suppressor_;

  base::ScopedObservation<PermissionChipInterface,
                          PermissionChipInterface::Observer>
      observation_{this};
  base::WeakPtrFactory<PermissionDashboardController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_CONTROLLER_H_
