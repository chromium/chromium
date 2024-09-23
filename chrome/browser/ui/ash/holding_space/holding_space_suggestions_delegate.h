// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_SUGGESTIONS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_SUGGESTIONS_DELEGATE_H_

#include <map>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"

namespace ash {

// A holding space delegate that manages the file suggestions (i.e. the files
// predicted to be needed by users) used in the holding space. The delegate
// observes the file suggestion service. When file suggestions update, the
// delegate refreshes the file suggestion items in the holding space model.
class HoldingSpaceSuggestionsDelegate
    : public HoldingSpaceKeyedServiceDelegate,
      public FileSuggestKeyedService::Observer {
 public:
  HoldingSpaceSuggestionsDelegate(HoldingSpaceKeyedService* service,
                                  HoldingSpaceModel* model);
  HoldingSpaceSuggestionsDelegate(const HoldingSpaceSuggestionsDelegate&) =
      delete;
  HoldingSpaceSuggestionsDelegate& operator=(
      const HoldingSpaceSuggestionsDelegate&) = delete;
  ~HoldingSpaceSuggestionsDelegate() override;

  // Refreshes suggestions.  Note that this intentionally does *not* invalidate
  // the file suggest service's item suggest cache which is too expensive for
  // holding space to invalidate.
  void RefreshSuggestions();

  // Removes suggestions associated with the specified `absolute_file_paths`.
  void RemoveSuggestions(
      const std::vector<base::FilePath>& absolute_file_paths);

 private:
  // HoldingSpaceKeyedServiceDelegate:
  void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;
  void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) override;
  void OnPersistenceRestored() override;

  // FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(FileSuggestionType type) override;

  // Fetches file suggestions of the specified `type` from the service. Returns
  // early if the fetch on the suggestions of `type` is already pending.
  void MaybeFetchSuggestions(FileSuggestionType type);

  // Maybe schedules a task to update suggestions in the holding space model.
  void MaybeScheduleUpdateSuggestionsInModel();

  // Called when fetching file suggestions finishes.
  void OnSuggestionsFetched(
      FileSuggestionType type,
      const std::optional<std::vector<FileSuggestData>>& suggestions);

  // Updates suggestions in the holding space model. The method ensures that:
  // 1. Drive file suggestions (if any) are always in front of local file
  // suggestions; and
  // 2. The suggestions of the same type (i.e. drive file ones or local file
  // ones) follow the relevance order.
  void UpdateSuggestionsInModel();

  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};

  // Records the suggestion types on which data fetches are pending.
  std::set<FileSuggestionType> pending_fetches_;

  // Caches the suggested files in the holding space model. In each key-value
  // pair: the key is a holding space suggestion item type; the value is an
  // array of paths to the suggested files. NOTE: each file path array follows
  // the relevance order, which means that a file path with a smaller index in
  // the array has a higher relevance score.
  std::map<HoldingSpaceItem::Type, std::vector<base::FilePath>>
      suggestions_by_type_;

  // Used to schedule the task of updating suggestions in model.
  base::OneShotTimer suggestion_update_timer_;

  base::WeakPtrFactory<HoldingSpaceSuggestionsDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_SUGGESTIONS_DELEGATE_H_
