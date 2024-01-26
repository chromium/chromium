// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_COMMERCE_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_COMMERCE_COMMERCE_PAGE_ACTION_CONTROLLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

class GURL;

namespace commerce {

// The possible ways a suggested save location can be handled. These must be
// kept in sync with the values in enums.xml.
enum class PageActionIconInteractionState {
  // The icon was shown and the user clicked it.
  kClicked = 0,

  // The icon was shown and expanded before the user clicked on it.
  kClickedExpanded = 1,

  // The icon was shown but the user did not interact with it.
  kNotClicked = 2,

  // The icon was shown and expanded but the user did not interact with it.
  kNotClickedExpanded = 3,

  // This enum must be last and is only used for histograms.
  kMaxValue = kNotClickedExpanded
};

class CommercePageActionController {
 public:
  explicit CommercePageActionController(
      base::RepeatingCallback<void()> host_update_callback);
  CommercePageActionController(const CommercePageActionController&) = delete;
  CommercePageActionController& operator=(const CommercePageActionController&) =
      delete;
  virtual ~CommercePageActionController();

  // Returns whether the UI should show for the current navigation. A nullopt
  // implies that the UI does not have enough information to definitively
  // respond true or false.
  virtual std::optional<bool> ShouldShowForNavigation() = 0;

  // Whether the page action wants to expand, based on its own rules.
  virtual bool WantsExpandedUi() = 0;

  // Automatically called when a relevant event on the active web contents
  // happens.
  virtual void ResetForNewNavigation(const GURL& url) = 0;

 protected:
  // Notify the host that is coordinating the icons that some state in the
  // implementation has changed.
  void NotifyHost();

 private:
  void RunHostUpdateCallback();

  base::RepeatingCallback<void()> host_update_callback_;

  base::WeakPtrFactory<CommercePageActionController> weak_factory_{this};
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_COMMERCE_PAGE_ACTION_CONTROLLER_H_
