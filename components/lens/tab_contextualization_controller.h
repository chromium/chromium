// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_TAB_CONTEXTUALIZATION_CONTROLLER_H_
#define COMPONENTS_LENS_TAB_CONTEXTUALIZATION_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/lens/contextual_input.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace lens {

class TabContextualizationController {
 public:
  TabContextualizationController();
  explicit TabContextualizationController(tabs::TabInterface* tab);
  ~TabContextualizationController();

  DECLARE_USER_DATA(TabContextualizationController);
  static TabContextualizationController* From(tabs::TabInterface* tab);

  // Callback type alias for when the page context is retrieved.
  using GetPageContextCallback = base::OnceCallback<void(
      std::unique_ptr<lens::ContextualInputData> page_content_data)>;

  // Callback type alias for when the page context eligibility is retrieved.
  using GetPageContextEligibilityCallback =
      base::OnceCallback<void(bool page_context_eligible)>;

  // Gets contextual page content for the tab.
  void GetPageContext(GetPageContextCallback callback);

  // Triggers initial page context eligibility check on the current page.
  // Equivalent to calling `optimization_guide::IsPageContextEligible()` with
  // empty frame_metadata. Only needed for the Chromnient use case, lower
  // priority to implement.
  void GetInitialPageContextEligibility(
      GetPageContextEligibilityCallback callback);

  // Returns whether the page is context eligible based on the latest cached
  // state. If the page context eligibility API has not been loaded, this will
  // return false. Only needed for the Chromnient use case, lower priority to
  // implement.
  bool GetCurrentPageContextEligibility();

 private:
  ::ui::ScopedUnownedUserData<TabContextualizationController>
      scoped_unowned_user_data_;
};

}  // namespace lens

#endif  // COMPONENTS_LENS_TAB_CONTEXTUALIZATION_CONTROLLER_H_
