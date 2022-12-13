// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_REALBOX_REALBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_REALBOX_REALBOX_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/omnibox.mojom.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/url_formatter/spoof_checks/idna_metrics.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;
class MetricsReporter;
class Profile;

namespace content {
class WebContents;
class WebUIDataSource;
}  // namespace content

namespace gfx {
struct VectorIcon;
}  // namespace gfx

// Handles bidirectional communication between NTP realbox JS and the browser.
class RealboxHandler : public omnibox::mojom::PageHandler,
                       public AutocompleteController::Observer {
 public:
  enum class FocusState {
    // kNormal means the row is focused, and Enter key navigates to the match.
    kFocusedMatch,

    // kFocusedButtonRemoveSuggestion state means the Remove Suggestion (X)
    // button is focused. Pressing enter will attempt to remove this suggestion.
    kFocusedButtonRemoveSuggestion,
  };

  static void SetupWebUIDataSource(content::WebUIDataSource* source,
                                   Profile* profile);
  static std::string AutocompleteMatchVectorIconToResourceName(
      const gfx::VectorIcon& icon);
  static std::string PedalVectorIconToResourceName(const gfx::VectorIcon& icon);

  RealboxHandler(
      mojo::PendingReceiver<omnibox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      MetricsReporter* metrics_reporter);

  RealboxHandler(const RealboxHandler&) = delete;
  RealboxHandler& operator=(const RealboxHandler&) = delete;

  ~RealboxHandler() override;

  // omnibox::mojom::PageHandler:
  void SetPage(mojo::PendingRemote<omnibox::mojom::Page> pending_page) override;
  void QueryAutocomplete(const std::u16string& input,
                         bool prevent_inline_autocomplete) override;
  void StopAutocomplete(bool clear_result) override;
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             base::TimeDelta time_elapsed_since_last_focus,
                             uint8_t mouse_button,
                             bool alt_key,
                             bool ctrl_key,
                             bool meta_key,
                             bool shift_key) override;
  void DeleteAutocompleteMatch(uint8_t line) override;
  void ToggleSuggestionGroupIdVisibility(int32_t suggestion_group_id) override;
  void LogCharTypedToRepaintLatency(base::TimeDelta latency) override;
  void ExecuteAction(uint8_t line,
                     base::TimeTicks match_selection_timestamp,
                     uint8_t mouse_button,
                     bool alt_key,
                     bool ctrl_key,
                     bool meta_key,
                     bool shift_key) override;
  void OnNavigationLikely(
      uint8_t line,
      omnibox::mojom::NavigationPredictor navigation_predictor) override;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // OpenURL function used as a callback for execution of actions.
  void OpenURL(const GURL& destination_url,
               TemplateURLRef::PostContent* post_content,
               WindowOpenDisposition disposition,
               ui::PageTransition transition,
               AutocompleteMatchType::Type type,
               base::TimeTicks match_selection_timestamp,
               bool destination_url_entered_without_scheme,
               const std::u16string&,
               const AutocompleteMatch&,
               const AutocompleteMatch&,
               IDNA2008DeviationCharacter);

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<AutocompleteController> autocomplete_controller_;
  base::ScopedObservation<OmniboxControllerEmitter,
                          AutocompleteController::Observer>
      controller_emitter_observation_{this};
  base::TimeTicks time_user_first_modified_realbox_;
  raw_ptr<MetricsReporter> metrics_reporter_;

  mojo::Remote<omnibox::mojom::Page> page_;
  mojo::Receiver<omnibox::mojom::PageHandler> page_handler_;

  base::WeakPtrFactory<RealboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_REALBOX_REALBOX_HANDLER_H_
