// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/bubble_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/autofill/bubble_controller_base.h"
#include "chrome/browser/ui/autofill/bubble_manager_impl.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// static
std::unique_ptr<BubbleManager> BubbleManager::Create(tabs::TabInterface* tab) {
  return base::WrapUnique<BubbleManager>(new BubbleManagerImpl(tab));
}

// static
BubbleManager* BubbleManager::GetForWebContents(
    content::WebContents* web_contents) {
  return GetForTab(tabs::TabInterface::MaybeGetFromContents(web_contents));
}

// static
BubbleManager* BubbleManager::GetForTab(tabs::TabInterface* tab_interface) {
  CHECK(base::FeatureList::IsEnabled(
      autofill::features::kAutofillShowBubblesBasedOnPriorities));
  if (!tab_interface) {
    return nullptr;
  }

  tabs::TabFeatures* tab_features = tab_interface->GetTabFeatures();
  if (!tab_features) {
    // The TabFeatures object might be destroyed during WebContents teardown.
    return nullptr;
  }

  return tab_features->autofill_bubble_manager();
}

}  // namespace autofill
