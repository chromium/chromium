// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/search/search_engine_base_url_tracker.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class Profile;

// BrowserInstantController is responsible for reloading any Instant tabs (which
// today just means NTPs) when the default search provider changes. This can
// happen when the user chooses a different default search engine, or when the
// Google base URL changes while Google is the default search engine.
class BrowserInstantController {
 public:
  DECLARE_USER_DATA(BrowserInstantController);

  explicit BrowserInstantController(BrowserWindowInterface& browser_window);

  BrowserInstantController(const BrowserInstantController&) = delete;
  BrowserInstantController& operator=(const BrowserInstantController&) = delete;

  ~BrowserInstantController();

  // Returns the controller for `browser_window`, or null if `browser_window` is
  // null or has none.
  static BrowserInstantController* From(BrowserWindowInterface* browser_window);

 private:
  void OnSearchEngineBaseURLChanged(
      SearchEngineBaseURLTracker::ChangeReason change_reason);

  const raw_ptr<Profile> profile_;
  const raw_ref<BrowserWindowInterface> browser_window_;

  std::unique_ptr<SearchEngineBaseURLTracker> search_engine_base_url_tracker_;

  ui::ScopedUnownedUserData<BrowserInstantController> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
