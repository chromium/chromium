// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/discards/discards_ui.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/browser/ui/webui/discards/graph_dump_impl.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom-forward.h"
#include "chrome/browser/ui/webui/discards/site_data_provider_impl.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/discards_resources.h"
#include "chrome/grit/discards_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/performance_manager/public/features.h"
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
#include "url/gurl.h"
#include "url/origin.h"

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
  NOTREACHED_IN_MIGRATION();
  return discards::mojom::LifecycleUnitVisibility::VISIBLE;
#endif
}

resource_coordinator::LifecycleUnit* GetLifecycleUnitById(int32_t id) {
  for (resource_coordinator::LifecycleUnit* lifecycle_unit :
       g_browser_process->GetTabManager()->GetSortedLifecycleUnits()) {
    if (lifecycle_unit->GetID() == id)
      return lifecycle_unit;
  }
  return nullptr;
}

double GetSiteEngagementScore(content::WebContents* contents) {
  // Get the active navigation entry. Restored tabs should always have one.
  auto& controller = contents->GetController();
  const int current_entry_index = controller.GetCurrentEntryIndex();

  // A WebContents which hasn't navigated yet does not have a NavigationEntry.
  if (current_entry_index == -1)
    return 0;

  auto* nav_entry = controller.GetEntryAtIndex(current_entry_index);
  DCHECK(nav_entry);

  auto* engagement_svc = site_engagement::SiteEngagementService::Get(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  return engagement_svc->GetDetails(nav_entry->GetURL()).total_score;
}

class DiscardsDetailsProviderImpl : public discards::mojom::DetailsProvider {
 public:
  // This instance is deleted when the supplied pipe is destroyed.
  explicit DiscardsDetailsProviderImpl(
      mojo::PendingReceiver<discards::mojom::DetailsProvider> receiver)
      : receiver_(this, std::move(receiver)) {}

  DiscardsDetailsProviderImpl(const DiscardsDetailsProviderImpl&) = delete;
  DiscardsDetailsProviderImpl& operator=(const DiscardsDetailsProviderImpl&) =
      delete;

  ~DiscardsDetailsProviderImpl() override {}

  // discards::mojom::DetailsProvider overrides:
  void GetTabDiscardsInfo(GetTabDiscardsInfoCallback callback) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    const resource_coordinator::LifecycleUnitVector lifecycle_units =
        tab_manager->GetSortedLifecycleUnits();

    std::vector<discards::mojom::TabDiscardsInfoPtr> infos;
    infos.reserve(lifecycle_units.size());

    const base::TimeTicks now = resource_coordinator::NowTicks();

    // Convert the LifecycleUnits to a vector of TabDiscardsInfos.
    size_t rank = 1;
    for (resource_coordinator::LifecycleUnit* lifecycle_unit :
         lifecycle_units) {
      discards::mojom::TabDiscardsInfoPtr info(
          discards::mojom::TabDiscardsInfo::New());

      resource_coordinator::TabLifecycleUnitExternal*
          tab_lifecycle_unit_external =
              lifecycle_unit->AsTabLifecycleUnitExternal();
      content::WebContents* contents =
          tab_lifecycle_unit_external->GetWebContents();

      info->tab_url = contents->GetLastCommittedURL().spec();
      info->title = base::UTF16ToUTF8(lifecycle_unit->GetTitle());
      info->visibility =
          GetLifecycleUnitVisibility(lifecycle_unit->GetVisibility());
      info->loading_state = lifecycle_unit->GetLoadingState();
      info->state = lifecycle_unit->GetState();
      resource_coordinator::DecisionDetails discard_details;
      info->can_discard = lifecycle_unit->CanDiscard(
          ::mojom::LifecycleUnitDiscardReason::PROACTIVE, &discard_details);
      info->cannot_discard_reasons = discard_details.GetFailureReasonStrings();
      info->discard_reason = lifecycle_unit->GetDiscardReason();
      info->discard_count = lifecycle_unit->GetDiscardCount();
      info->utility_rank = rank++;
      const base::TimeTicks last_focused_time =
          lifecycle_unit->GetLastFocusedTimeTicks();
      const base::TimeDelta elapsed =
          (last_focused_time == base::TimeTicks::Max())
              ? base::TimeDelta()
              : (now - last_focused_time);
      info->last_active_seconds = static_cast<int32_t>(elapsed.InSeconds());
      info->is_auto_discardable =
          tab_lifecycle_unit_external->IsAutoDiscardable();
      info->id = lifecycle_unit->GetID();
      info->site_engagement_score = GetSiteEngagementScore(contents);
      info->state_change_time =
          lifecycle_unit->GetStateChangeTime() - base::TimeTicks::UnixEpoch();
      // TODO(crbug.com/41409267): The focus is used to compute the page
      // lifecycle state. This should be replaced with the actual page lifecycle
      // state information from Blink, but this depends on implementing the
      // passive state and plumbing it to the browser.
      info->has_focus = lifecycle_unit->GetLastFocusedTimeTicks().is_max();

      infos.push_back(std::move(info));
    }

    std::move(callback).Run(std::move(infos));
  }

  void SetAutoDiscardable(int32_t id,
                          bool is_auto_discardable,
                          SetAutoDiscardableCallback callback) override {
    auto* lifecycle_unit = GetLifecycleUnitById(id);
    if (lifecycle_unit) {
      auto* tab_lifecycle_unit_external =
          lifecycle_unit->AsTabLifecycleUnitExternal();
      if (tab_lifecycle_unit_external)
        tab_lifecycle_unit_external->SetAutoDiscardable(is_auto_discardable);
    }
    std::move(callback).Run();
  }

  void DiscardById(int32_t id,
                   mojom::LifecycleUnitDiscardReason reason,
                   DiscardByIdCallback callback) override {
    auto* lifecycle_unit = GetLifecycleUnitById(id);
    if (lifecycle_unit) {
      // Callback to do the discard with the memory estimate.
      auto discard_callback = base::BindOnce(
          [](int32_t id, mojom::LifecycleUnitDiscardReason reason,
             DiscardByIdCallback post_discard_callback,
             uint64_t memory_estimate) {
            // Look up lifecycle_unit by id again, in case it's deleted while
            // waiting.
            auto* lifecycle_unit = GetLifecycleUnitById(id);
            if (lifecycle_unit) {
              lifecycle_unit->Discard(reason, memory_estimate);
            }
            std::move(post_discard_callback).Run();
          },
          id, reason, std::move(callback));

      performance_manager::user_tuning::
          GetDiscardedMemoryEstimateForWebContents(
              lifecycle_unit->AsTabLifecycleUnitExternal()->GetWebContents(),
              std::move(discard_callback));
    }
  }

  void LoadById(int32_t id) override {
    auto* lifecycle_unit = GetLifecycleUnitById(id);
    if (lifecycle_unit)
      lifecycle_unit->Load();
  }

  void Discard(DiscardCallback callback) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    tab_manager->DiscardTab(mojom::LifecycleUnitDiscardReason::URGENT);
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
    performance_manager::user_tuning::PerformanceDetectionManager::GetInstance()
        ->ForceTabCpuDataRefresh();
  }

 private:
  mojo::Receiver<discards::mojom::DetailsProvider> receiver_;
};

}  // namespace

DiscardsUI::DiscardsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIDiscardsHost);

  source->AddBoolean(
      "isPerformanceInterventionDemoModeEnabled",
      base::FeatureList::IsEnabled(
          performance_manager::features::kPerformanceInterventionDemoMode));

  webui::SetupWebUIDataSource(
      source, base::make_span(kDiscardsResources, kDiscardsResourcesSize),
      IDR_DISCARDS_DISCARDS_HTML);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  profile_id_ = profile->UniqueId();
}

WEB_UI_CONTROLLER_TYPE_IMPL(DiscardsUI)

DiscardsUI::~DiscardsUI() {}

void DiscardsUI::BindInterface(
    mojo::PendingReceiver<discards::mojom::DetailsProvider> receiver) {
  ui_handler_ =
      std::make_unique<DiscardsDetailsProviderImpl>(std::move(receiver));
}

void DiscardsUI::BindInterface(
    mojo::PendingReceiver<discards::mojom::SiteDataProvider> receiver) {
  if (performance_manager::PerformanceManager::IsAvailable()) {
    // Forward the interface receiver directly to the service.
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce(&SiteDataProviderImpl::CreateAndBind,
                                  std::move(receiver), profile_id_));
  }
}

void DiscardsUI::BindInterface(
    mojo::PendingReceiver<discards::mojom::GraphDump> receiver) {
  if (performance_manager::PerformanceManager::IsAvailable()) {
    // Forward the interface receiver directly to the service.
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce(&DiscardsGraphDumpImpl::CreateAndBind,
                                  std::move(receiver)));
  }
}
