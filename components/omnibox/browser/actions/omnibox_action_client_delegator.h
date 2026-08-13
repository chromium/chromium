// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CLIENT_DELEGATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CLIENT_DELEGATOR_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "url/gurl.h"

// A delegator implementation of `OmniboxAction::Client` that forwards all calls
// to another `OmniboxAction::Client` instance (`delegate_`).
class OmniboxActionClientDelegator : public OmniboxAction::Client {
 public:
  explicit OmniboxActionClientDelegator(OmniboxAction::Client& delegate);
  OmniboxActionClientDelegator(const OmniboxActionClientDelegator&) = delete;
  OmniboxActionClientDelegator& operator=(const OmniboxActionClientDelegator&) =
      delete;
  ~OmniboxActionClientDelegator() override;

  // OmniboxAction::Client:
  void OpenSharingHub() override;
  void NewIncognitoWindow() override;
  void OpenIncognitoClearBrowsingDataDialog() override;
  void CloseIncognitoWindows() override;
  void PromptPageTranslation() override;
  bool OpenJourneys(const std::string& query) override;
  void OpenLensOverlay(
      bool show,
      lens::LensOverlayInvocationSource invocation_source) override;
  bool ShouldOpenCoBrowsePanel() const override;
  void OpenCoBrowsePanel() override;
  void IssueContextualSearchRequest(const GURL& destination_url,
                                    AutocompleteMatchType::Type match_type,
                                    bool is_zero_prefix_suggestion) override;
  bool ShouldOpenComposeboxForAskG() const override;
  void OpenComposeboxForAskG() override;

 protected:
  const raw_ptr<OmniboxAction::Client> delegate_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_CLIENT_DELEGATOR_H_
