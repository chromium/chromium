// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_PAGE_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class AutocompleteController;
class Profile;

// Implementation of mojo::OmniboxPageHandler.  StartOmniboxQuery() calls to a
// private AutocompleteController. It also listens for updates from the
// AutocompleteController to OnResultChanged() and passes those results to
// the OmniboxPage.
class OmniboxPageHandler : public AutocompleteControllerDelegate,
                           public mojom::OmniboxPageHandler,
                           public OmniboxControllerEmitter::Observer {
 public:
  // OmniboxPageHandler is deleted when the supplied pipe is destroyed.
  OmniboxPageHandler(Profile* profile,
                     mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver);
  ~OmniboxPageHandler() override;

  // AutocompleteControllerDelegate overrides:
  void OnResultChanged(bool default_match_changed) override;

  // OmniboxControllerEmitter::Observer overrides:
  void OnOmniboxQuery(AutocompleteController* controller,
                      const AutocompleteInput& input) override;
  void OnOmniboxResultChanged(bool default_match_changed,
                              AutocompleteController* controller) override;

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

 private:
  // Looks up whether the hostname is a typed host (i.e., has received
  // typed visits).  Return true if the lookup succeeded; if so, the
  // value of |is_typed_host| is set appropriately.
  bool LookupIsTypedHost(const base::string16& host, bool* is_typed_host) const;

  // Re-initializes the AutocompleteController in preparation for the
  // next query.
  void ResetController();

  // The omnibox AutocompleteController that collects/sorts/dup-
  // eliminates the results as they come in.
  std::unique_ptr<AutocompleteController> controller_;

  // Time the user's input was sent to the omnibox to start searching.
  // Needed because we also pass timing information in the object we
  // hand back to the javascript.
  base::Time time_omnibox_started_;

  // The input used when starting the AutocompleteController.
  AutocompleteInput input_;

  // Handle back to the page by which we can pass results.
  mojo::Remote<mojom::OmniboxPage> page_;

  // The Profile* handed to us in our constructor.
  Profile* profile_;

  mojo::Receiver<mojom::OmniboxPageHandler> receiver_;

  ScopedObserver<OmniboxControllerEmitter, OmniboxControllerEmitter::Observer>
      observer_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_PAGE_HANDLER_H_
