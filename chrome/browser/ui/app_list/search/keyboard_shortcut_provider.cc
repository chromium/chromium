// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_provider.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/keyboard_shortcut_item.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/keyboard_shortcut_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

namespace app_list {

namespace {

using chromeos::string_matching::TokenizedString;

constexpr size_t kMaxResults = 10;
constexpr double kResultRelevanceThreshold = 0.5;

}  // namespace

KeyboardShortcutProvider::KeyboardShortcutProvider(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ProcessShortcutList();
}

KeyboardShortcutProvider::~KeyboardShortcutProvider() = default;

void KeyboardShortcutProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&KeyboardShortcutProvider::Search, base::Unretained(this)),
      base::BindOnce(&KeyboardShortcutProvider::OnSearchComplete,
                     weak_factory_.GetWeakPtr()));
}

ash::AppListSearchResultType KeyboardShortcutProvider::ResultType() const {
  return ash::AppListSearchResultType::kKeyboardShortcut;
}

void KeyboardShortcutProvider::ProcessShortcutList() {
  DCHECK(shortcut_data_.empty());
  for (const auto& item :
       keyboard_shortcut_viewer::GetKeyboardShortcutItemList()) {
    shortcut_data_.push_back(KeyboardShortcutData(item));
  }
}

KeyboardShortcutProvider::ShortcutDataAndScores
KeyboardShortcutProvider::Search() {
  // Find all shortcuts which meet the relevance threshold.
  KeyboardShortcutProvider::ShortcutDataAndScores candidates;
  for (const auto& shortcut : shortcut_data_) {
    double relevance = KeyboardShortcutResult::CalculateRelevance(
        last_tokenized_query_.value(), shortcut.description);
    if (relevance > kResultRelevanceThreshold) {
      candidates.push_back(std::make_pair(shortcut, relevance));
    }
  }
  return candidates;
}

void KeyboardShortcutProvider::OnSearchComplete(
    KeyboardShortcutProvider::ShortcutDataAndScores candidates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Sort candidates by descending relevance score.
  std::sort(candidates.begin(), candidates.end(),
            [](auto& a, auto& b) { return a.second > b.second; });

  // Convert final candidates into correct type, and publish.
  SearchProvider::Results results;
  for (size_t i = 0; i < std::min(candidates.size(), kMaxResults); ++i) {
    results.push_back(std::make_unique<KeyboardShortcutResult>(
        profile_, candidates[i].first, candidates[i].second));
  }
  SwapResults(&results);
}

}  // namespace app_list
