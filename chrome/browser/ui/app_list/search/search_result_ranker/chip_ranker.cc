// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/chip_ranker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

namespace app_list {
namespace {

constexpr int kNumChips = 5;

// Strings used for the ranked types in the RecurrenceRanker.
constexpr char kApp[] = "app";
constexpr char kDriveFile[] = "drive";
constexpr char kLocalFile[] = "local";

// A small number that we expect to be smaller than the difference between the
// scores of any two results. This means it can be used to insert a result A
// between results B and C by setting A's score to B's score + kScoreEpsilon.
constexpr float kScoreEpsilon = 1e-5f;

void SortHighToLow(std::vector<Mixer::SortData*>* results) {
  std::sort(results->begin(), results->end(),
            [](const Mixer::SortData* const a, const Mixer::SortData* const b) {
              return a->score > b->score;
            });
}

float GetScore(const std::map<std::string, float>& scores,
               const std::string& key) {
  const auto it = scores.find(key);
  // We expect to always find a score for |key| in |scores|, because the ranker
  // is initialized with some default scores. However a state without scores is
  // possible, eg. if the recurrence ranker file is corrupted. In this case,
  // default the score to 0.
  if (it == scores.end()) {
    return 0.0f;
  }
  return it->second;
}

int MinDriveChips(const int available_chips) {
  return available_chips >= 5 ? 2 : 1;
}

int MinLocalChips(const int available_chips) {
  return available_chips >= 5 ? 1 : 0;
}

int MinAppChips(const int available_chips) {
  return available_chips >= 4 ? 2 : 1;
}

void InitializeRanker(RecurrenceRanker* ranker) {
  // This initialization starts with two apps, two drive files, and one local
  // file if there are five available chips. It will start with two apps, one
  // drive, and one local file if there are only four chips. Apps are left in a
  // close second place, so the first click of an app will replace one drive
  // file with one app.
  ranker->Record(kLocalFile);
  ranker->Record(kApp);
  ranker->Record(kDriveFile);
  ranker->Record(kDriveFile);
}

}  // namespace

ChipRanker::ChipRanker(Profile* profile) : profile_(profile) {
  DCHECK(profile);

  // Set up ranker model. This is tuned close to MRU.
  RecurrenceRankerConfigProto config;
  config.set_min_seconds_between_saves(240u);
  config.set_condition_limit(1u);
  config.set_condition_decay(0.5f);
  config.set_target_limit(5u);
  config.set_target_decay(0.9f);
  config.mutable_predictor()->mutable_default_predictor();

  type_ranker_ = std::make_unique<RecurrenceRanker>(
      "", profile_->GetPath().AppendASCII("suggested_files_ranker.pb"), config,
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile_));
}

ChipRanker::~ChipRanker() = default;

void ChipRanker::Train(const AppLaunchData& app_launch_data) {
  const auto type = app_launch_data.ranking_item_type;
  switch (type) {
    case RankingItemType::kApp:
      type_ranker_->Record(kApp);
      break;
    case RankingItemType::kDriveQuickAccessChip:
    case RankingItemType::kDriveQuickAccess:
      type_ranker_->Record(kDriveFile);
      break;
    case RankingItemType::kZeroStateFileChip:
    case RankingItemType::kZeroStateFile:
      type_ranker_->Record(kLocalFile);
      break;
    default:
      break;
  }
}

void ChipRanker::Rank(Mixer::SortedResults* results) {
  // Construct lists of pointers for each ranked result type, sorted in
  // decreasing score order.
  std::vector<Mixer::SortData*> app_results;
  std::vector<Mixer::SortData*> drive_results;
  std::vector<Mixer::SortData*> local_results;
  for (auto& result : *results) {
    switch (result.result->result_type()) {
      case ash::AppListSearchResultType::kInstalledApp:
      case ash::AppListSearchResultType::kInternalApp:
        app_results.emplace_back(&result);
        break;
      case ash::AppListSearchResultType::kFileChip:
        local_results.emplace_back(&result);
        break;
      case ash::AppListSearchResultType::kDriveQuickAccessChip:
        drive_results.emplace_back(&result);
        break;
      default:
        break;
    }
  }

  SortHighToLow(&app_results);
  SortHighToLow(&drive_results);
  SortHighToLow(&local_results);

  if (drive_results.empty() && local_results.empty()) {
    return;
  }

  // If this is the first initialization of the ranker, warm it up with some
  // default scores for apps and files.
  if (type_ranker_->empty()) {
    InitializeRanker(type_ranker_.get());
  }

  const int drive_size = static_cast<int>(drive_results.size());
  const int local_size = static_cast<int>(local_results.size());
  const int apps_size = static_cast<int>(app_results.size());

  // Get the per-type scores from the ranker and calculate the score decrement.
  const auto ranks = type_ranker_->Rank();
  float app_score = GetScore(ranks, kApp);
  float drive_score = GetScore(ranks, kDriveFile);
  float local_score = GetScore(ranks, kLocalFile);
  int free_chips = NumAvailableChips(results);
  const float score_delta =
      (app_score + drive_score + local_score) / std::max(1, free_chips);

  // Allocate as many of the per-type minimum chips as possible.
  int num_drive = std::min(MinDriveChips(free_chips), drive_size);
  int num_local = std::min(MinLocalChips(free_chips), local_size);
  int num_apps = std::min(MinAppChips(free_chips), apps_size);

  // Decrement the scores to reflect the minimum results added previously, and
  // update the number of free chips accordingly.
  app_score -= num_apps * score_delta;
  drive_score -= num_drive * score_delta;
  local_score -= num_local * score_delta;
  free_chips = free_chips - num_drive - num_local - num_apps;

  // Allocate the remaining 'free' chips. When there aren't results enough of
  // one type to fill the number of chips deserved by that type's score, fall
  // back to filling with another type in the order: drive -> local -> app.
  for (int i = 0; i < free_chips; ++i) {
    if (num_drive < drive_size && drive_score > app_score &&
        drive_score > local_score) {
      drive_score -= score_delta;
      ++num_drive;
    } else if (num_local < local_size && local_score > app_score) {
      local_score -= score_delta;
      ++num_local;
    } else if (num_apps < apps_size) {
      app_score -= score_delta;
      ++num_apps;
    }
  }

  // Set result scores to make the final list of results. Use the score of the
  // lowest-scoring shown app as the baseline for file results.
  double current_score = 1.0;
  if (0 < num_apps && num_apps <= apps_size) {
    current_score = app_results[num_apps - 1]->score;
  }

  // Score the Drive results just below that lowest app. Set unshown results to
  // 0.0 to ensure they don't interfere.
  for (int i = 0; i < drive_size; ++i) {
    if (i < num_drive) {
      current_score -= kScoreEpsilon;
      drive_results[i]->score = current_score;
    } else {
      drive_results[i]->score = 0.0;
    }
  }

  // Score the local file results just below that lowest Drive result.
  for (int i = 0; i < local_size; ++i) {
    if (i < num_local) {
      current_score -= kScoreEpsilon;
      local_results[i]->score = current_score;
    } else {
      local_results[i]->score = 0.0;
    }
  }

  // Score unshown apps below the lowest local file result to ensure apps with
  // identical scores don't crowd out files. We cannot score these as 0.0
  // because the ranking is used in other views. This preserves order, if not
  // the scores themselves.
  for (int i = num_apps; i < apps_size; ++i) {
    current_score -= kScoreEpsilon;
    app_results[i]->score = current_score;
  }
}

int ChipRanker::NumAvailableChips(Mixer::SortedResults* results) {
  int num_chips = kNumChips;

  for (const auto& result : *results) {
    const auto type = result.result->result_type();
    if (type == ash::AppListSearchResultType::kAssistantChip ||
        type == ash::AppListSearchResultType::kPlayStoreReinstallApp ||
        IsSuggestionChip(result.result->id(), profile_)) {
      --num_chips;
    }
  }

  return std::max(0, num_chips);
}

RecurrenceRanker* ChipRanker::GetRankerForTest() {
  return type_ranker_.get();
}

}  // namespace app_list
