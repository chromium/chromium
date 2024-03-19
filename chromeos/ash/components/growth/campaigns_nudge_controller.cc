// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_nudge_controller.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"

namespace growth {

namespace {

// Nudge payload paths.
inline constexpr char kNudgeTitle[] = "title";
inline constexpr char kNudgeBody[] = "body";

const std::string* GetNudgeTitle(const NudgePayload* nudge_payload) {
  CHECK(nudge_payload);
  return nudge_payload->FindString(kNudgeTitle);
}

const std::string* GetNudgeBody(const NudgePayload* nudge_payload) {
  CHECK(nudge_payload);
  return nudge_payload->FindString(kNudgeBody);
}

}  // namespace

CampaignsNudgeController::CampaignsNudgeController() = default;

CampaignsNudgeController::~CampaignsNudgeController() = default;

void CampaignsNudgeController::ShowNudge(const NudgePayload* nudge_payload) {
  CHECK(nudge_payload);

  auto* body_text = GetNudgeBody(nudge_payload);
  CHECK(body_text);

  std::u16string nudge_body = base::UTF8ToUTF16(*body_text);

  // TODO: b/329893738 - Create unique id for different growth nudges.
  const std::string id = "campaign_nudge";

  // TODO: b/329701489 - Getting nudge anchor view.
  auto nudge_data = ash::AnchoredNudgeData(
      id, ash::NudgeCatalogName::kGrowthCampaignNudge, nudge_body,
      /*anchor_view=*/nullptr);

  auto* title = GetNudgeTitle(nudge_payload);
  if (title && !title->empty()) {
    nudge_data.title_text = base::UTF8ToUTF16(*title);
  }

  // TODO: b/329701349 - Update nudge duration.
  nudge_data.duration = ash::NudgeDuration::kMediumDuration;
  ash::Shell::Get()->anchored_nudge_manager()->Show(nudge_data);
}

}  // namespace growth
