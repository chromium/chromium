// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/webui/resources/cr_components/searchbox/searchbox.mojom.h"

class MetricsReporter;
class OmniboxController;
class Profile;
class OmniboxEditModel;

namespace content {
class WebContents;
class WebUIDataSource;
}  // namespace content

// Base class for browser-side handlers that handle bi-directional communication
// with WebUI search boxes.
class SearchboxHandler : public searchbox::mojom::PageHandler,
                         public AutocompleteController::Observer {
 public:
  static void SetupWebUIDataSource(content::WebUIDataSource* source,
                                   Profile* profile,
                                   bool enable_voice_search = false,
                                   bool enable_lens_search = false);
  static std::string AutocompleteMatchVectorIconToResourceName(
      const gfx::VectorIcon& icon);
  static std::string ActionVectorIconToResourceName(
      const gfx::VectorIcon& icon);

  // Returns true if the page remote is bound and ready to receive calls.
  bool IsRemoteBound() const;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // searchbox::mojom::PageHandler:
  void SetPage(
      mojo::PendingRemote<searchbox::mojom::Page> pending_page) override;
  void OnFocusChanged(bool focused) override;
  void QueryAutocomplete(const std::u16string& input,
                         bool prevent_inline_autocomplete) override;
  void StopAutocomplete(bool clear_result) override;
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             uint8_t mouse_button,
                             bool alt_key,
                             bool ctrl_key,
                             bool meta_key,
                             bool shift_key) override;
  void OnNavigationLikely(
      uint8_t line,
      const GURL& url,
      omnibox::mojom::NavigationPredictor navigation_predictor) override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest, AutocompleteController_Start);
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest, RealboxUpdatesEditModelInput);
  FRIEND_TEST_ALL_PREFIXES(LensSearchboxHandlerTest,
                           Lens_AutocompleteController_Start);

  SearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      MetricsReporter* metrics_reporter);
  SearchboxHandler(const SearchboxHandler&) = delete;
  SearchboxHandler& operator=(const SearchboxHandler&) = delete;
  ~SearchboxHandler() override;

  OmniboxController* omnibox_controller() const;
  AutocompleteController* autocomplete_controller() const;
  OmniboxEditModel* edit_model() const;

  const AutocompleteMatch* GetMatchWithUrl(size_t index, const GURL& url);

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<MetricsReporter> metrics_reporter_;
  raw_ptr<OmniboxController> controller_;
  std::unique_ptr<OmniboxController> owned_controller_;

  base::ScopedObservation<AutocompleteController,
                          AutocompleteController::Observer>
      autocomplete_controller_observation_{this};

  // Since mojo::Remote is not thread-safe, use an atomic to signal readiness.
  std::atomic<bool> page_set_;
  mojo::Receiver<searchbox::mojom::PageHandler> page_handler_;
  mojo::Remote<searchbox::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_HANDLER_H_
