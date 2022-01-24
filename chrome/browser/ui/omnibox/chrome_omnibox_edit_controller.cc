// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"

#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#endif

void ChromeOmniboxEditController::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match) {
  TRACE_EVENT("omnibox", "ChromeOmniboxEditController::OnAutocompleteAccept",
              "text", text, "match", match, "alternative_nav_match",
              alternative_nav_match);
  OmniboxEditController::OnAutocompleteAccept(
      destination_url, post_content, disposition, transition, match_type,
      match_selection_timestamp, destination_url_entered_without_scheme, text,
      match, alternative_nav_match);

  auto navigation = chrome::OpenCurrentURL(browser_);

  ChromeOmniboxNavigationObserver::Create(navigation.get(), profile_, text,
                                          match, alternative_nav_match);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MaybeShowExtensionControlledSearchNotification(
      GetWebContents(), match_type);
#endif
}

void ChromeOmniboxEditController::OnInputInProgress(bool in_progress) {
  UpdateWithoutTabRestore();
}

content::WebContents* ChromeOmniboxEditController::GetWebContents() {
  return nullptr;
}

void ChromeOmniboxEditController::UpdateWithoutTabRestore() {}

ChromeOmniboxEditController::ChromeOmniboxEditController(
    Browser* browser,
    Profile* profile,
    CommandUpdater* command_updater)
    : browser_(browser), profile_(profile), command_updater_(command_updater) {}

ChromeOmniboxEditController::~ChromeOmniboxEditController() {}
