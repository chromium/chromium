// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/discards_ui.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/browser/ui/webui/discards/graph_dump_impl.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom-forward.h"
#include "chrome/browser/ui/webui/discards/site_data_provider_impl.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/discards_resources.h"
#include "chrome/grit/discards_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/freezing/cannot_freeze_reason.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/resources/grit/ui_resources_map.h"
#include "ui/webui/webui_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_DESKTOP_ANDROID)
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#endif  // !BUILDFLAG(IS_DESKTOP_ANDROID)

using performance_manager::PageNode;
using performance_manager::policies::DiscardEligibilityPolicy;

namespace {

discards::mojom::LifecycleUnitVisibility GetLifecycleUnitVisibility(
    content::Visibility visibility) {
  switch (visibility) {
    case content::Visibility::HIDDEN:
      return discards::mojom::LifecycleUnitVisibility::HIDDEN;
    case content::Visibility::OCCLUDED:
      return discards::mojom::LifecycleUnitVisibility::OCCLUDED;
    case content::Visibility::VISIBLE:
      return discards::mojom::LifecycleUnitVisibility::VISIBLE;
  }
#if defined(COMPILER_MSVC)
  NOTREACHED();
#endif
}

double GetSiteEngagementScore(content::WebContents* contents) {
  // Get the active navigation entry. Restored tabs should always have one.
  auto& controller = contents->GetController();
  const int current_entry_index = controller.GetCurrentEntryIndex();

  // A WebContents which hasn't navigated yet does not have a NavigationEntry.
  if (current_entry_index == -1) {
    return 0;
  }

  auto* nav_entry = controller.GetEntryAtIndex(current_entry_index);
  DCHECK(nav_entry);

  auto* engagement_svc = site_engagement::SiteEngagementService::Get(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  return engagement_svc->GetDetails(nav_entry->GetURL()).total_score;
}

mojom::LifecycleUnitLoadingState GetLifecycleUnitLoadingState(
    PageNode::LoadingState loading_state) {
  switch (loading_state) {
    case PageNode::LoadingState::kLoadingNotStarted:
    case PageNode::LoadingState::kLoadingTimedOut:
      return mojom::LifecycleUnitLoadingState::UNLOADED;

    case PageNode::LoadingState::kLoading:
      return mojom::LifecycleUnitLoadingState::LOADING;

    case PageNode::LoadingState::kLoadedBusy:
    case PageNode::LoadingState::kLoadedIdle:
      return mojom::LifecycleUnitLoadingState::LOADED;
  }
}

#if !BUILDFLAG(IS_ANDROID)
discards::mojom::CanFreeze ToCanFreezeMojom(
    performance_manager::freezing::CanFreeze can_freeze) {
  switch (can_freeze) {
    case performance_manager::freezing::CanFreeze::kYes:
      return discards::mojom::CanFreeze::YES;
    case performance_manager::freezing::CanFreeze::kNo:
      return discards::mojom::CanFreeze::NO;
    case performance_manager::freezing::CanFreeze::kVaries:
      return discards::mojom::CanFreeze::VARIES;
  }
  NOTREACHED();
}

std::vector<std::string> ToCannotFreezeReasonsStrings(
    const performance_manager::freezing::CanFreezeDetails& details) {
  std::vector<std::string> reasons;
  reasons.reserve(details.cannot_freeze_reasons.size() +
                  details.cannot_freeze_reasons_connected_pages.size());
  for (auto reason : details.cannot_freeze_reasons) {
    reasons.push_back(
        performance_manager::freezing::CannotFreezeReasonToString(reason));
  }
  for (auto reason : details.cannot_freeze_reasons_connected_pages) {
    reasons.push_back(base::StringPrintf(
        "%s (from connected page)",
        performance_manager::freezing::CannotFreezeReasonToString(reason)));
  }
  return reasons;
}
#endif  // !BUILDFLAG(IS_ANDROID)

class DiscardsDetailsProviderImpl
    : public discards::mojom::DetailsProvider,
      public performance_manager::GraphOwnedDefaultImpl {
 public:
  // This instance is deleted when the supplied pipe is destroyed.
  explicit DiscardsDetailsProviderImpl(
      mojo::PendingReceiver<discards::mojom::DetailsProvider> receiver)
      : receiver_(this, std::move(receiver)) {}

  DiscardsDetailsProviderImpl(const DiscardsDetailsProviderImpl&) = delete;
  DiscardsDetailsProviderImpl& operator=(const DiscardsDetailsProviderImpl&) =
      delete;

  ~DiscardsDetailsProviderImpl() override = default;

  // discards::mojom::DetailsProvider overrides:
  void GetTabDiscardsInfo(GetTabDiscardsInfoCallback callback) override {
    std::vector<discards::mojom::TabDiscardsInfoPtr> infos;

    DiscardEligibilityPolicy* eligiblity_policy =
        DiscardEligibilityPolicy::GetFromGraph(GetOwningGraph());
    DCHECK(eligiblity_policy);

    std::vector<performance_manager::policies::PageNodeSortProxy> candidates;
    for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
      if (page_node->GetType() != performance_manager::PageType::kTab) {
        continue;
      }
      performance_manager::policies::CanDiscardResult can_discard_result =
          eligiblity_policy->CanDiscard(
              page_node, DiscardEligibilityPolicy::DiscardReason::URGENT);
      candidates.emplace_back(page_node->GetWeakPtr(), can_discard_result,
                              page_node->IsVisible(), page_node->IsFocused(),
                              page_node->GetLastVisibilityChangeTime());
    }

    // Sorts with ascending importance.
    std::sort(candidates.begin(), candidates.end());

    page_nodes_by_id_.clear();

    int32_t rank = 1;
    int32_t id = 1;
    for (auto& candidate : candidates) {
      discards::mojom::TabDiscardsInfoPtr info(
          discards::mojom::TabDiscardsInfo::New());

      const base::WeakPtr<const PageNode> page_node = candidate.page_node();
      content::WebContents* contents = page_node->GetWebContents().get();
      CHECK(contents);

      info->tab_url = contents->GetLastCommittedURL().spec();
      info->title = base::UTF16ToUTF8(contents->GetTitle());
      info->visibility = GetLifecycleUnitVisibility(contents->GetVisibility());
      info->loading_state =
          GetLifecycleUnitLoadingState(page_node->GetLoadingState());

      info->cannot_discard_reasons =
          performance_manager::user_tuning::GetCannotDiscardReasonsForPageNode(
              page_node.get());
      info->can_discard = info->cannot_discard_reasons.empty();

#if BUILDFLAG(IS_ANDROID)
      info->cannot_freeze_reasons = {"not implemented"};
      info->can_freeze = discards::mojom::CanFreeze::NO;
#else
      // TODO(crbug.com/40160563): Add FreezingPolicy to Android.
      const auto can_freeze_details =
          performance_manager::freezing::GetCanFreezeDetailsForPageNode(
              page_node.get());
      info->cannot_freeze_reasons =
          ToCannotFreezeReasonsStrings(can_freeze_details);
      info->can_freeze = ToCanFreezeMojom(can_freeze_details.can_freeze);
#endif  // BUILDFLAG(IS_ANDROID)

      info->utility_rank = rank++;
      info->id = id++;
      page_nodes_by_id_.insert(std::make_pair(info->id, page_node));
      const auto* live_state_data =
          performance_manager::PageLiveStateDecorator::Data::FromPageNode(
              page_node.get());
      if (live_state_data) {
        info->is_auto_discardable = live_state_data->IsAutoDiscardable();
      }
      info->site_engagement_score = GetSiteEngagementScore(contents);
      info->has_focus = page_node->IsFocused();

#if !BUILDFLAG(IS_DESKTOP_ANDROID)
      auto* lifecycle_unit_external = resource_coordinator::
          TabLifecycleUnitSource::GetTabLifecycleUnitExternal(contents);
      // A TabLifecycleUnitExternal object is always a TabLifecycleUnit object.
      // TabLifecycleUnit will be removed (crbug.com/394889323).
      resource_coordinator::TabLifecycleUnitSource::TabLifecycleUnit*
          lifecycle_unit = static_cast<
              resource_coordinator::TabLifecycleUnitSource::TabLifecycleUnit*>(
              lifecycle_unit_external);
      if (lifecycle_unit) {
        info->state = lifecycle_unit->GetState();
        info->discard_reason = lifecycle_unit->GetDiscardReason();
        info->discard_count = lifecycle_unit->GetDiscardCount();
        const base::TimeTicks last_focused_time =
            lifecycle_unit->GetLastFocusedTimeTicks();
        const base::TimeDelta elapsed =
            (last_focused_time == base::TimeTicks::Max())
                ? base::TimeDelta()
                : (resource_coordinator::NowTicks() - last_focused_time);
        info->last_active_seconds = static_cast<int32_t>(elapsed.InSeconds());
        info->state_change_time =
            lifecycle_unit->GetStateChangeTime() - base::TimeTicks::UnixEpoch();
      }
#endif  // !BUILDFLAG(IS_DESKTOP_ANDROID)

      infos.push_back(std::move(info));
    }

    std::move(callback).Run(std::move(infos));
  }

  void SetAutoDiscardable(int32_t id,
                          bool is_auto_discardable,
                          SetAutoDiscardableCallback callback) override {
    auto it = page_nodes_by_id_.find(id);
    if (it != page_nodes_by_id_.end()) {
      content::WebContents* contents = it->second->GetWebContents().get();
      CHECK(contents);
      performance_manager::PageLiveStateDecorator::SetIsAutoDiscardable(
          contents, is_auto_discardable);
    }
    std::move(callback).Run();
  }

  void DiscardById(int32_t id,
                   mojom::LifecycleUnitDiscardReason reason,
                   DiscardByIdCallback callback) override {
    auto it = page_nodes_by_id_.find(id);
    if (it != page_nodes_by_id_.end() && it->second) {
      const PageNode* page_node = it->second.get();
      performance_manager::user_tuning::DiscardPage(
          page_node, reason,
          /*ignore_minimum_time_in_background=*/true);
    }
    std::move(callback).Run();
  }

  void FreezeById(int32_t id) override {
    auto it = page_nodes_by_id_.find(id);
    if (it != page_nodes_by_id_.end() && it->second) {
      const PageNode* page_node = it->second.get();
      content::WebContents* contents = page_node->GetWebContents().get();
      CHECK(contents);
      contents->SetPageFrozen(true);
    }
  }

  void LoadById(int32_t id) override {
    auto it = page_nodes_by_id_.find(id);
    if (it != page_nodes_by_id_.end() && it->second) {
      const PageNode* page_node = it->second.get();
      PageNode::LoadingState loading_state = page_node->GetLoadingState();
      if (loading_state != PageNode::LoadingState::kLoadingNotStarted &&
          loading_state != PageNode::LoadingState::kLoadingTimedOut) {
        return;
      }

      content::WebContents* contents = page_node->GetWebContents().get();
      CHECK(contents);
      contents->GetController().SetNeedsReload();
      contents->GetController().LoadIfNecessary();
      contents->Focus();
    }
  }

  void Discard(DiscardCallback callback) override {
    performance_manager::user_tuning::DiscardAnyPage(
        mojom::LifecycleUnitDiscardReason::URGENT,
        /*ignore_minimum_time_in_background=*/true);
    std::move(callback).Run();
  }

  void ToggleBatterySaverMode() override {
    performance_manager::user_tuning::prefs::BatterySaverModeState state =
        performance_manager::user_tuning::prefs::
            GetCurrentBatterySaverModeState(g_browser_process->local_state());
    g_browser_process->local_state()->SetInteger(
        performance_manager::user_tuning::prefs::kBatterySaverModeState,
        static_cast<int>(state == performance_manager::user_tuning::prefs::
                                      BatterySaverModeState::kDisabled
                             ? performance_manager::user_tuning::prefs::
                                   BatterySaverModeState::kEnabled
                             : performance_manager::user_tuning::prefs::
                                   BatterySaverModeState::kDisabled));
  }

  void RefreshPerformanceTabCpuMeasurements() override {
#if !BUILDFLAG(IS_DESKTOP_ANDROID)
    performance_manager::user_tuning::PerformanceDetectionManager::GetInstance()
        ->ForceTabCpuDataRefresh();
#endif  // !BUILDFLAG(IS_DESKTOP_ANDROID)
  }

 private:
  mojo::Receiver<discards::mojom::DetailsProvider> receiver_;

  // Mapping from id to page node.
  base::flat_map<int32_t, base::WeakPtr<const PageNode>> page_nodes_by_id_;
};

}  // namespace

DiscardsUI::DiscardsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIDiscardsHost);

  bool demoModeEnabled = false;
#if !BUILDFLAG(IS_DESKTOP_ANDROID)
  demoModeEnabled = base::FeatureList::IsEnabled(
      performance_manager::features::kPerformanceInterventionDemoMode);
#endif  // !BUILDFLAG(IS_DESKTOP_ANDROID)
  source->AddBoolean("isPerformanceInterventionDemoModeEnabled",
                     demoModeEnabled);

  webui::SetupWebUIDataSource(source, kDiscardsResources,
                              IDR_DISCARDS_DISCARDS_HTML);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  profile_id_ = profile->UniqueId();
}

WEB_UI_CONTROLLER_TYPE_IMPL(DiscardsUI)

DiscardsUI::~DiscardsUI() = default;

void DiscardsUI::BindInterface(
    mojo::PendingReceiver<discards::mojom::DetailsProvider> receiver) {
  performance_manager::PerformanceManager::GetGraph()->PassToGraph(
      std::make_unique<DiscardsDetailsProviderImpl>(std::move(receiver)));
}

void DiscardsUI::BindInterface(
    mojo::PendingReceiver<discards::mojom::SiteDataProvider> receiver) {
  if (performance_manager::PerformanceManager::IsAvailable()) {
    // Forward the interface receiver directly to the service.
    SiteDataProviderImpl::CreateAndBind(
        std::move(receiver), profile_id_,
        performance_manager::PerformanceManager::GetGraph());
  }
}

void DiscardsUI::BindInterface(
    mojo::PendingReceiver<discards::mojom::GraphDump> receiver) {
  if (performance_manager::PerformanceManager::IsAvailable()) {
    // Forward the interface receiver directly to the service.
    DiscardsGraphDumpImpl::CreateAndBind(
        std::move(receiver),
        performance_manager::PerformanceManager::GetGraph());
  }
}
