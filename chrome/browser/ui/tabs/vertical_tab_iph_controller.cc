// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/vertical_tab_iph_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"

namespace {
// The minimum number of tabs required in the tab strip before the vertical
// tabs IPH is considered for display.
constexpr int kMinTabsForVerticalTabPromo = 20;

// Delay before showing the IPH to avoid interrupting the user's immediate flow.
constexpr base::TimeDelta kPromoDelay = base::Seconds(3);

// The ratio of current tab width to standard width below which we suggest
// vertical tabs.
constexpr float kTabShrinkageThreshold = 0.5f;

}  // namespace

DEFINE_USER_DATA(VerticalTabIphController);

VerticalTabIphController::VerticalTabIphController(
    BrowserWindowInterface* interface)
    : browser_window_interface_(interface),
      scoped_data_(interface->GetUnownedUserDataHost(), *this) {
  auto* tab_strip_model = browser_window_interface_->GetTabStripModel();
  auto* profile = Profile::FromBrowserContext(tab_strip_model->profile());
  if (!profile->GetPrefs()->GetBoolean(prefs::kVerticalTabsEnabledFirstTime)) {
    browser_window_interface_->GetTabStripModel()->AddObserver(this);
  }
}

VerticalTabIphController::~VerticalTabIphController() = default;

void VerticalTabIphController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // We check the criteria whenever a tab is added or the selection changes
  if (change.type() == TabStripModelChange::kInserted) {
    promo_timer_.Start(FROM_HERE, kPromoDelay, this,
                       &VerticalTabIphController::MaybeShowPromo);
  }
}

void VerticalTabIphController::MaybeShowPromo() {
  auto* tab_strip_model = browser_window_interface_->GetTabStripModel();
  if (tab_strip_model->count() <= kMinTabsForVerticalTabPromo) {
    return;
  }

  auto* profile = Profile::FromBrowserContext(tab_strip_model->profile());
  if (profile->GetPrefs()->GetBoolean(prefs::kVerticalTabsEnabledFirstTime)) {
    return;
  }

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window_interface_);
  if (!browser_view || !browser_view->tab_strip_view()) {
    return;
  }

  int first_unpinned_index = tab_strip_model->IndexOfFirstNonPinnedTab();
  if (first_unpinned_index == tab_strip_model->count()) {
    return;
  }

  // If the unpinned tab is shrunk significantly compared to its standard
  // width show the IPH.
  views::View* tab =
      browser_view->tab_strip_view()->GetTabAnchorViewAt(first_unpinned_index);
  if (IsTabShrunk(tab)) {
    BrowserUserEducationInterface::From(browser_window_interface_)
        ->MaybeShowFeaturePromo(
            feature_engagement::kIPHVerticalTabstripTutorialFeature);
  }
}

bool VerticalTabIphController::IsTabShrunk(views::View* tab) {
  if (!tab) {
    return false;
  }

  const int current_width = tab->width();
  const int standard_width = tab->GetPreferredSize().width();

  if (standard_width <= 0) {
    return false;
  }

  const float shrinkage_ratio =
      static_cast<float>(current_width) / standard_width;
  return shrinkage_ratio < kTabShrinkageThreshold;
}
