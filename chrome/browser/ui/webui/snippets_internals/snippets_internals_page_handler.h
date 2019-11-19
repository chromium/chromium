// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_SNIPPETS_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_SNIPPETS_INTERNALS_PAGE_HANDLER_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals.mojom.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// TODO: Write tests for this.
class SnippetsInternalsPageHandler
    : public snippets_internals::mojom::PageHandler,
      public ntp_snippets::ContentSuggestionsService::Observer {
 public:
  explicit SnippetsInternalsPageHandler(
      mojo::PendingReceiver<snippets_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<snippets_internals::mojom::Page> page,
      ntp_snippets::ContentSuggestionsService* content_suggestions_service,
      PrefService* pref_service);
  ~SnippetsInternalsPageHandler() override;

  // snippets_internals::mojom::PageHandler
  void GetGeneralProperties(GetGeneralPropertiesCallback) override;
  void GetUserClassifierProperties(
      GetUserClassifierPropertiesCallback) override;
  void ClearUserClassifierProperties() override;
  void GetCategoryRankerProperties(
      GetCategoryRankerPropertiesCallback) override;
  void ReloadSuggestions() override;
  void ClearCachedSuggestions() override;
  void GetRemoteContentSuggestionsProperties(
      GetRemoteContentSuggestionsPropertiesCallback) override;
  void FetchSuggestionsInBackground(
      int64_t,
      FetchSuggestionsInBackgroundCallback) override;
  void GetLastJson(GetLastJsonCallback) override;
  void GetSuggestionsByCategory(GetSuggestionsByCategoryCallback) override;
  void ClearDismissedSuggestions(int64_t) override;

 private:
  // ntp_snippets::ContentSuggestionsService::Observer:
  void OnNewSuggestions(ntp_snippets::Category category) override;
  void OnCategoryStatusChanged(
      ntp_snippets::Category category,
      ntp_snippets::CategoryStatus new_status) override;
  void OnSuggestionInvalidated(
      const ntp_snippets::ContentSuggestion::ID& suggestion_id) override;
  void OnFullRefreshRequired() override;
  void ContentSuggestionsServiceShutdown() override;

  void FetchSuggestionsInBackgroundImpl(FetchSuggestionsInBackgroundCallback);
  void GetSuggestionsByCategoryImpl(GetSuggestionsByCategoryCallback);

  // Misc. methods.
  void CollectDismissedSuggestions(
      int last_index,
      GetSuggestionsByCategoryCallback callback,
      std::vector<ntp_snippets::ContentSuggestion> suggestions);

  // Receiver from the mojo interface to concrete impl.
  mojo::Receiver<snippets_internals::mojom::PageHandler> receiver_;

  // Observer to notify frontend of dirty data.
  ScopedObserver<ntp_snippets::ContentSuggestionsService,
                 ntp_snippets::ContentSuggestionsService::Observer>
      content_suggestions_service_observer_;

  // Services that provide the data & functionality.
  ntp_snippets::ContentSuggestionsService* content_suggestions_service_;
  ntp_snippets::RemoteSuggestionsProvider* remote_suggestions_provider_;
  PrefService* pref_service_;

  // Store dismissed suggestions in an instance variable during aggregation
  std::map<ntp_snippets::Category,
           std::vector<ntp_snippets::ContentSuggestion>,
           ntp_snippets::Category::CompareByID>
      dismissed_suggestions_;

  // Timers to delay actions.
  base::OneShotTimer suggestion_fetch_timer_;

  // Handle back to the page by which we can update.
  mojo::Remote<snippets_internals::mojom::Page> page_;

  base::WeakPtrFactory<SnippetsInternalsPageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SnippetsInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SNIPPETS_INTERNALS_SNIPPETS_INTERNALS_PAGE_HANDLER_H_
