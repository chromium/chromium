// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_model_delegate.h"

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
#include "content/public/browser/navigation_handle.h"
#include "extensions/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#endif

ChromeOmniboxEditModelDelegate::ChromeOmniboxEditModelDelegate(
    Browser* browser,
    Profile* profile,
    CommandUpdater* command_updater)
    : browser_(browser), profile_(profile), command_updater_(command_updater) {}

ChromeOmniboxEditModelDelegate::~ChromeOmniboxEditModelDelegate() = default;

void ChromeOmniboxEditModelDelegate::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match,
    IDNA2008DeviationCharacter deviation_char_in_hostname) {
  TRACE_EVENT("omnibox", "ChromeOmniboxEditModelDelegate::OnAutocompleteAccept",
              "text", text, "match", match, "alternative_nav_match",
              alternative_nav_match);

  destination_url_ = destination_url;
  post_content_ = post_content;
  disposition_ = disposition;
  transition_ = transition;
  match_selection_timestamp_ = match_selection_timestamp;
  destination_url_entered_without_scheme_ =
      destination_url_entered_without_scheme;

  if (browser_) {
    auto navigation = chrome::OpenCurrentURL(browser_);
    ChromeOmniboxNavigationObserver::Create(navigation.get(), profile_, text,
                                            match, alternative_nav_match);

    // If this navigation was typed by the user and the hostname contained an
    // IDNA 2008 deviation character, record a UKM. See idn_spoof_checker.h
    // for details about deviation characters.
    if (deviation_char_in_hostname != IDNA2008DeviationCharacter::kNone) {
      ukm::SourceId source_id = ukm::ConvertToSourceId(
          navigation->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
      ukm::builders::Navigation_IDNA2008Transition(source_id)
          .SetCharacter(static_cast<int>(deviation_char_in_hostname))
          .Record(ukm::UkmRecorder::Get());
    }
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MaybeShowExtensionControlledSearchNotification(GetWebContents(),
                                                             match_type);
#endif
}

void ChromeOmniboxEditModelDelegate::OnInputInProgress(bool in_progress) {
  UpdateWithoutTabRestore();
}
