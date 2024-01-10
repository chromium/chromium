// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/standalone_window_migration_nudge_util.h"

#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr char kNudgeId[] = "migration_ux_nudge_id";

}

void CreateAndShowNudge(views::View* anchor_view, std::u16string view_title) {
  ash::AnchoredNudgeData nudge = ash::AnchoredNudgeData(
      kNudgeId, NudgeCatalogName::kStandaloneWindowMigrationUx,
      l10n_util::GetStringUTF16(IDS_STANDALONE_WINDOW_MIGRATION_UX_BUBBLE_TEXT),
      anchor_view);
  nudge.arrow = views::BubbleBorder::BOTTOM_RIGHT;
  nudge.title_text = l10n_util::GetStringFUTF16(
      IDS_STANDALONE_WINDOW_MIGRATION_UX_BUBBLE_HEADING, view_title);
  nudge.primary_button_text =
      l10n_util::GetStringUTF16(IDS_STANDALONE_WINDOW_MIGRATION_UX_GOT_IT);
  nudge.secondary_button_text = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  nudge.secondary_button_callback =
      base::BindRepeating(&OnLearnMoreButtonSelected);

  ash::AnchoredNudgeManager::Get()->Show(nudge);
}

void OnLearnMoreButtonSelected() {
  // TODO(b/319328360): Implement. Details of this callback are not yet
  // finalized. This callback should link the user to a help document.
}

}  // namespace ash
