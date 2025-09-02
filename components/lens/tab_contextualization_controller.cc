// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/tab_contextualization_controller.h"

#include "content/public/browser/navigation_handle.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace lens {

DEFINE_USER_DATA(TabContextualizationController);

TabContextualizationController::TabContextualizationController(
    tabs::TabInterface* tab)
    : content::WebContentsObserver(tab->GetContents()),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this),
      tab_(tab) {
  tab_subscription_ = tab->RegisterWillDiscardContents(
      base::BindRepeating(&TabContextualizationController::WillDiscardContents,
                          base::Unretained(this)));
}

TabContextualizationController::~TabContextualizationController() = default;

void TabContextualizationController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
}

TabContextualizationController* TabContextualizationController::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void TabContextualizationController::PrimaryPageChanged(content::Page& page) {
  is_page_context_eligible_ = false;
}

void TabContextualizationController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Call the asynchronous eligibility check.
  GetInitialPageContextEligibility(
      base::BindOnce(&TabContextualizationController::OnEligibilityChecked,
                     base::Unretained(this)));
}

void TabContextualizationController::OnEligibilityChecked(
    bool is_page_context_eligible) {
  is_page_context_eligible_ = is_page_context_eligible;
}

// TODO(crbug.com/439595898): Get contextual page content.
void TabContextualizationController::GetPageContext(
    GetPageContextCallback callback) {}

// TODO(crbug.com/439597165): Check tab eligibility
void TabContextualizationController::GetInitialPageContextEligibility(
    GetPageContextEligibilityCallback callback) {}

bool TabContextualizationController::GetCurrentPageContextEligibility() {
  return is_page_context_eligible_;
}

}  // namespace lens
