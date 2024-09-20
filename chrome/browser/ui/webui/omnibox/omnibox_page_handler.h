// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_PAGE_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class AutocompleteController;
class AutocompleteResult;
class AutocompleteScoringModelService;
class Profile;

// Implementation of mojo::OmniboxPageHandler.  StartOmniboxQuery() calls to a
// private AutocompleteController. It also listens for updates from the
// AutocompleteController to OnResultChanged() and passes those results to
// the OmniboxPage.
class OmniboxPageHandler : public AutocompleteController::Observer,
                           public mojom::OmniboxPageHandler {
 public:
  // OmniboxPageHandler is deleted when the supplied pipe is destroyed.
  OmniboxPageHandler(Profile* profile,
                     mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver);

  OmniboxPageHandler(const OmniboxPageHandler&) = delete;
  OmniboxPageHandler& operator=(const OmniboxPageHandler&) = delete;

  ~OmniboxPageHandler() override;

  // AutocompleteController::Observer overrides:
  void OnStart(AutocompleteController* controller,
               const AutocompleteInput& input) override;
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;
  void OnMlScored(AutocompleteController* controller,
                  const AutocompleteResult& result) override;

  // mojom::OmniboxPageHandler overrides:
  void SetClientPage(mojo::PendingRemote<mojom::OmniboxPage> page) override;
  // current_url may be invalid, in which case, autocomplete input's url won't
  // be set.
  void StartOmniboxQuery(const std::string& input_string,
                         bool reset_autocomplete_controller,
                         int32_t cursor_position,
                         bool zero_suggest,
                         bool prevent_inline_autocomplete,
                         bool prefer_keyword,
                         const std::string& current_url,
                         int32_t page_classification) override;
  void GetMlModelVersion(GetMlModelVersionCallback callback) override;
  void StartMl(mojom::SignalsPtr signals, StartMlCallback callback) override;

 private:
  void OnBitmapFetched(mojom::AutocompleteControllerType type,
                       const std::string& image_url,
                       const SkBitmap& bitmap);

  // Looks up whether the hostname is a typed host (i.e., has received
  // typed visits).  Return true if the lookup succeeded; if so, the
  // value of |is_typed_host| is set appropriately.
  bool LookupIsTypedHost(const std::u16string& host, bool* is_typed_host) const;

  // Creates an `AutocompleteController` for `controller_` or
  // `ml_disabled_controller_`.
  std::unique_ptr<AutocompleteController> CreateController(bool ml_disabled);

  // Compares `controller` with `controller_` & `ml_disabled_controller_`.
  mojom::AutocompleteControllerType GetAutocompleteControllerType(
      AutocompleteController* controller);

  // Helper to get the ML service for this profile.
  AutocompleteScoringModelService* GetMlService();

  // A controller to allow chrome://omnibox can try inputs without messing with
  // location bar omnibox.
  std::unique_ptr<AutocompleteController> controller_;
  // A controller with ML disabled to allow chrome://omnibox/ml to show a
  // before-after comparison.
  std::unique_ptr<AutocompleteController> ml_disabled_controller_;

  // Time the user's input was sent to the omnibox to start searching.
  // Needed because we also pass timing information in the object we
  // hand back to the javascript.
  base::Time time_omnibox_started_;

  // The input used when starting the AutocompleteController.
  AutocompleteInput input_;

  // Handle back to the page by which we can pass results.
  mojo::Remote<mojom::OmniboxPage> page_;

  // The Profile* handed to us in our constructor.
  raw_ptr<Profile> profile_;

  mojo::Receiver<mojom::OmniboxPageHandler> receiver_;

  base::ScopedObservation<AutocompleteControllerEmitter,
                          AutocompleteController::Observer>
      observation_{this};

  base::WeakPtrFactory<OmniboxPageHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_PAGE_HANDLER_H_
