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

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest, AutocompleteController_Start);
  FRIEND_TEST_ALL_PREFIXES(RealboxHandlerTest,
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
