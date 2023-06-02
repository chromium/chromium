// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_CHIP_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_CHIP_TAB_HELPER_H_

#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// When a page in the background has been discarded due to high efficiency mode,
// and the user returns to that tab, a page action chip should be shown to the
// user which conveys information about the discarded tab to the user.
// The HighEfficiencyChipTabHelper is a per-tab class which manages the state of
// the high efficiency chip.
class HighEfficiencyChipTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HighEfficiencyChipTabHelper> {
 public:
  HighEfficiencyChipTabHelper(const HighEfficiencyChipTabHelper&) = delete;
  HighEfficiencyChipTabHelper& operator=(const HighEfficiencyChipTabHelper&) =
      delete;

  ~HighEfficiencyChipTabHelper() override;

  // Returns whether the chip associated with a discarded tab should be shown.
  bool ShouldChipBeVisible() const;

  // Returns whether the chip associated with a discarded tab should animate in.
  bool ShouldIconAnimate() const;

  // Indicates that the chip has been animated for the current discard.
  void SetWasAnimated();

  // Returns whether the tab associated with this helper has been navigated
  // away from and to another tab.
  bool HasChipBeenHidden();

  // Returns the memory savings (in bytes) of the previously discarded tab.
  uint64_t GetMemorySavingsInBytes() const;

  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  friend class content::WebContentsUserData<HighEfficiencyChipTabHelper>;
  explicit HighEfficiencyChipTabHelper(content::WebContents* contents);

  bool IsProactiveDiscard() const;

  bool was_discarded_ = false;
  bool was_animated_ = false;
  bool was_chip_hidden_ = false;
  bool is_site_supported_ = false;
  absl::optional<mojom::LifecycleUnitDiscardReason> discard_reason_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_HIGH_EFFICIENCY_CHIP_TAB_HELPER_H_
