// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/skills/public/skill_metrics.mojom.h"
#include "components/sync/protocol/skill_specifics.pb.h"

namespace skills {

namespace {

constexpr const char* GetSkillsDialogActionHistogramName(
    SkillsDialogEntryPoint entrypoint,
    bool is_edit_mode) {
  switch (entrypoint) {
    case SkillsDialogEntryPoint::kWebClientBlank:
      return is_edit_mode ? "Skills.Dialog.Edit.WebClient.Blank.Action"
                          : "Skills.Dialog.Creation.WebClient.Blank.Action";
    case SkillsDialogEntryPoint::kWebClientRemix:
      return is_edit_mode ? "Skills.Dialog.Edit.WebClient.Remix.Action"
                          : "Skills.Dialog.Creation.WebClient.Remix.Action";
    case SkillsDialogEntryPoint::kWebClientPrefilled:
      return is_edit_mode ? "Skills.Dialog.Edit.WebClient.Prefilled.Action"
                          : "Skills.Dialog.Creation.WebClient.Prefilled.Action";
    case SkillsDialogEntryPoint::kManagementPageBlank:
      return is_edit_mode
                 ? "Skills.Dialog.Edit.ManagementPage.Blank.Action"
                 : "Skills.Dialog.Creation.ManagementPage.Blank.Action";
    case SkillsDialogEntryPoint::kManagementPagePrefilled:
      return is_edit_mode
                 ? "Skills.Dialog.Edit.ManagementPage.Prefilled.Action"
                 : "Skills.Dialog.Creation.ManagementPage.Prefilled.Action";
    case SkillsDialogEntryPoint::kManagementPageRemix:
      return is_edit_mode
                 ? "Skills.Dialog.Edit.ManagementPage.Remix.Action"
                 : "Skills.Dialog.Creation.ManagementPage.Remix.Action";
    case SkillsDialogEntryPoint::kUnknown:
      break;
  }
  return is_edit_mode ? "Skills.Dialog.Edit.Unknown.Action"
                      : "Skills.Dialog.Creation.Unknown.Action";
}

constexpr const char* GetSkillsPageHistogramName(
    skills::mojom::SkillsManagementPage page) {
  switch (page) {
    case skills::mojom::SkillsManagementPage::kErrorPage:
      return "Skills.Management.ErrorPage.Action";
    case skills::mojom::SkillsManagementPage::kYourSkills:
      return "Skills.Management.YourSkills.Action";
    case skills::mojom::SkillsManagementPage::kBrowseSkills:
      return "Skills.Management.BrowseSkills.Action";
  }
  return "Skills.Management.UnknownPage.Action";
}

}  // namespace

bool IsEditMode(const skills::Skill* skill) {
  // If it has no value or id, it's a brand new blank skill.
  if (!skill || skill->id.empty()) {
    return false;
  }
  // If it's a raw 1P template, we are creating a remix from it.
  if (skill->source == sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY) {
    return false;
  }
  // Otherwise (User Created or Derived), we are editing an existing skill.
  return true;
}

SkillsDialogEntryPoint ResolveEntryPointForWebClient(
    const skills::Skill* skill) {
  // Brand new skill (no skill data exists yet).
  if (!skill || skill->id.empty()) {
    return SkillsDialogEntryPoint::kWebClientBlank;
  }

  // Skill from template or a user skill derived from a template.
  if (skill->source == sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY ||
      skill->source ==
          sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY) {
    return SkillsDialogEntryPoint::kWebClientRemix;
  }

  // Standard user skill.
  return SkillsDialogEntryPoint::kWebClientPrefilled;
}

SkillsDialogEntryPoint ResolveEntryPointForManagementPage(
    const skills::Skill* skill) {
  // Brand new skill (no skill data exists yet).
  if (!skill || skill->id.empty()) {
    return SkillsDialogEntryPoint::kManagementPageBlank;
  }

  // Skill from template or a user skill derived from a template.
  if (skill->source == sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY ||
      skill->source ==
          sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY) {
    return SkillsDialogEntryPoint::kManagementPageRemix;
  }

  // Standard user skill.
  return SkillsDialogEntryPoint::kManagementPagePrefilled;
}

void RecordSkillsDialogAction(SkillsDialogAction action,
                              SkillsDialogEntryPoint entrypoint,
                              bool is_edit_mode) {
  base::UmaHistogramEnumeration(
      GetSkillsDialogActionHistogramName(entrypoint, is_edit_mode), action);
  // Log the aggregate.
  base::UmaHistogramEnumeration(is_edit_mode ? "Skills.Dialog.Edit.Action"
                                             : "Skills.Dialog.Creation.Action",
                                action);
}

void RecordSkillsInvokeAction(SkillsInvokeAction action) {
  base::UmaHistogramEnumeration("Skills.Invoke.Action", action);
}

void RecordSkillsInvokeResult(SkillsInvokeResult result) {
  base::UmaHistogramEnumeration("Skills.Invoke.Result", result);
}

void RecordSkillsSaveResult(SkillsSaveResult result) {
  base::UmaHistogramEnumeration("Skills.Save.Result", result);
}

void RecordSkillsRefineResult(SkillsRefineResult result) {
  base::UmaHistogramEnumeration("Skills.Refine.Result", result);
}

void RecordUserSkillCount(size_t skill_count) {
  base::UmaHistogramCounts1000("Skills.UserSkills.Count",
                               base::checked_cast<int>(skill_count));
}

void RecordSkillsManagementAction(
    skills::mojom::SkillsManagementPage page,
    skills::mojom::SkillsManagementAction action) {
  base::UmaHistogramEnumeration(GetSkillsPageHistogramName(page), action);
}

void RecordSkillsFetchResult(SkillsFetchResult result) {
  base::UmaHistogramEnumeration("Skills.Downloader.FirstParty.FetchResult",
                                result);
}

void RecordSkillsHttpCode(int http_code) {
  base::UmaHistogramSparse("Skills.Downloader.FirstParty.HttpResponseCode",
                           http_code);
}

void RecordSkillsDownloadRequestStatus(SkillsDownloadRequestStatus status) {
  base::UmaHistogramEnumeration(
      "Skills.Management.FirstParty.DownloadRequestStatus", status);
}

void RecordSkillsManagementError(SkillsManagementError error) {
  base::UmaHistogramEnumeration("Skills.Management.Error", error);
}

}  // namespace skills
