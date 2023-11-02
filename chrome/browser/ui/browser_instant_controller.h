// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/search/search_engine_base_url_tracker.h"
#include "chrome/browser/ui/search/instant_controller.h"

#if BUILDFLAG(IS_ANDROID)
#error "Instant is only used on desktop";
#endif

class Browser;
class Profile;

// BrowserInstantController is responsible for reloading any Instant tabs (which
// today just means NTPs) when the default search provider changes. This can
// happen when the user chooses a different default search engine, or when the
// Google base URL changes while Google is the default search engine.
class BrowserInstantController {
 public:
  explicit BrowserInstantController(Browser* browser);

  BrowserInstantController(const BrowserInstantController&) = delete;
  BrowserInstantController& operator=(const BrowserInstantController&) = delete;

  ~BrowserInstantController();

 private:
  void OnSearchEngineBaseURLChanged(
      SearchEngineBaseURLTracker::ChangeReason change_reason);

  Profile* profile() const;

  const raw_ptr<Browser> browser_;

  InstantController instant_;

  std::unique_ptr<SearchEngineBaseURLTracker> search_engine_base_url_tracker_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_INSTANT_CONTROLLER_H_
