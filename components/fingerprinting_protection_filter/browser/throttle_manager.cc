// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/throttle_manager.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_child_navigation_throttle.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/subresource_filter/content/shared/browser/activation_state_computing_navigation_throttle.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/content/shared/common/utils.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/net_errors.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {
namespace {

using ::subresource_filter::ActivationDecision;
using ::subresource_filter::ActivationStateComputingNavigationThrottle;
using ::subresource_filter::AsyncDocumentSubresourceFilter;
using ::subresource_filter::GetSubresourceFilterRootPage;
using ::subresource_filter::IsInSubresourceFilterRoot;
using ::subresource_filter::ShouldInheritActivation;
using ::subresource_filter::ShouldInheritOpenerActivation;
using ::subresource_filter::ShouldInheritParentActivation;
using ::subresource_filter::VerifiedRuleset;
using ::subresource_filter::VerifiedRulesetDealer;

}  // namespace

// ==========UserData implementations==========

DOCUMENT_USER_DATA_KEY_IMPL(ThrottleManager::FilterHandle);

ThrottleManager::FilterHandle::FilterHandle(
    content::RenderFrameHost* rfh,
    std::unique_ptr<AsyncDocumentSubresourceFilter> filter)
    : DocumentUserData(rfh), filter_(std::move(filter)) {}

ThrottleManager::FilterHandle::~FilterHandle() = default;

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(
    ThrottleManager::ChildActivationThrottleHandle);

ThrottleManager::ChildActivationThrottleHandle::ChildActivationThrottleHandle(
    content::NavigationHandle& navigation_handle,
    ActivationStateComputingNavigationThrottle* throttle)
    : throttle_(throttle) {}

ThrottleManager::ChildActivationThrottleHandle::
    ~ChildActivationThrottleHandle() = default;

// ==========ThrottleManager implementation==========

// static
const int ThrottleManager::kUserDataKey;

// static
void ThrottleManager::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::FingerprintingProtectionHost>
        pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  if (auto* manager = FromPage(render_frame_host->GetPage())) {
    manager->receivers_.Bind(render_frame_host, std::move(pending_receiver));
  } else {
    for (auto navigation_handle :
         render_frame_host->GetPendingCommitCrossDocumentNavigations()) {
      // TODO(https://crbug.com/347304498): Add `ThrottleManagers` to
      // `RenderFrames` from creation time once activation is decoupled from
      // navigations.
      if ((manager = FromNavigationHandle(*navigation_handle))) {
        manager->receivers_.Bind(render_frame_host,
                                 std::move(pending_receiver));
        return;
      }
    }
  }
}

ThrottleManager::ThrottleManager(
    VerifiedRulesetDealer::Handle* dealer_handle,
    FingerprintingProtectionWebContentsHelper& web_contents_helper,
    content::NavigationHandle& initiating_navigation_handle,
    bool is_incognito)
    : receivers_(initiating_navigation_handle.GetWebContents(), this),
      ruleset_handle_(dealer_handle ? std::make_unique<VerifiedRuleset::Handle>(
                                          dealer_handle)
                                    : nullptr),
      web_contents_helper_(web_contents_helper),
      is_incognito_(is_incognito) {}

ThrottleManager::~ThrottleManager() {
  // All mojo callbacks must be run or their binding closed before they are
  // destroyed.
  web_contents_helper_->WillDestroyThrottleManager(this);
}

// static
std::unique_ptr<ThrottleManager> ThrottleManager::CreateForNewPage(
    VerifiedRulesetDealer::Handle* dealer_handle,
    FingerprintingProtectionWebContentsHelper& web_contents_helper,
    content::NavigationHandle& initiating_navigation_handle,
    bool is_incognito) {
  CHECK(IsInSubresourceFilterRoot(&initiating_navigation_handle));
  if (!features::IsFingerprintingProtectionFeatureEnabled()) {
    return nullptr;
  }

  return std::make_unique<ThrottleManager>(dealer_handle, web_contents_helper,
                                           initiating_navigation_handle,
                                           is_incognito);
}

// static
ThrottleManager* ThrottleManager::FromPage(content::Page& page) {
  return FingerprintingProtectionWebContentsHelper::GetThrottleManager(page);
}

// static
ThrottleManager* ThrottleManager::FromNavigationHandle(
    content::NavigationHandle& navigation_handle) {
  return FingerprintingProtectionWebContentsHelper::GetThrottleManager(
      navigation_handle);
}

void ThrottleManager::MaybeAppendNavigationThrottles(
    content::NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  CHECK(!navigation_handle->IsSameDocument());
  CHECK(!ShouldInheritActivation(navigation_handle->GetURL()));

  if (IsInSubresourceFilterRoot(navigation_handle)) {
    // Attempt to create root throttles.
    throttles->push_back(
        std::make_unique<FingerprintingProtectionPageActivationThrottle>(
            navigation_handle,
            web_contents_helper_->tracking_protection_settings(),
            web_contents_helper_->pref_service(), is_incognito_));
    auto activation_throttle =
        ActivationStateComputingNavigationThrottle::CreateForRoot(
            navigation_handle);
    ChildActivationThrottleHandle::CreateForNavigationHandle(
        *navigation_handle, activation_throttle.get());
    throttles->push_back(std::move(activation_throttle));
  } else {
    // Attempt to create child throttles.
    AsyncDocumentSubresourceFilter* parent_filter =
        GetParentFrameFilter(navigation_handle);
    if (parent_filter) {
      throttles->push_back(
          std::make_unique<FingerprintingProtectionChildNavigationThrottle>(
              navigation_handle, parent_filter,
              base::BindRepeating([](const GURL& url) {
                return base::StringPrintf(
                    kDisallowChildFrameConsoleMessageFormat,
                    url.possibly_invalid_spec().c_str());
              })));
      CHECK(ruleset_handle_);
      // TODO(https://crbug.com/346583606): Create a simpler passthrough
      // ActivationThrottle that defers child navigations until the parent
      // activation is computed and then forwards it to the child.
      auto activation_throttle =
          ActivationStateComputingNavigationThrottle::CreateForChild(
              navigation_handle, ruleset_handle_.get(),
              parent_filter->activation_state());
      CHECK(!ChildActivationThrottleHandle::GetForNavigationHandle(
          *navigation_handle));
      ChildActivationThrottleHandle::CreateForNavigationHandle(
          *navigation_handle, activation_throttle.get());
      throttles->push_back(std::move(activation_throttle));
    }
  }
}

// Pull the AsyncDocumentSubresourceFilter and its associated
// mojom::ActivationState out of the activation state computing throttle. Store
// it for later filtering of child frame navigations.
void ThrottleManager::ReadyToCommitInFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  std::ignore = ActivationStateForNextCommittedLoad(navigation_handle);
}

void ThrottleManager::DidFinishInFrameNavigation(
    content::NavigationHandle* navigation_handle,
    bool is_initial_navigation) {
  ActivationStateComputingNavigationThrottle* throttle = nullptr;
  ChildActivationThrottleHandle* throttle_handle =
      ChildActivationThrottleHandle::GetForNavigationHandle(*navigation_handle);
  if (throttle_handle) {
    throttle = throttle_handle->throttle();
    // Avoid dangling throttle pointers.
    ChildActivationThrottleHandle::DeleteForNavigationHandle(
        *navigation_handle);
  }

  // Do nothing if the frame was destroyed.
  if (navigation_handle->IsWaitingToCommit() &&
      navigation_handle->GetRenderFrameHost()->GetLifecycleState() ==
          content::RenderFrameHost::LifecycleState::kPendingDeletion) {
    return;
  }

  // TODO(https://crbug.com/40280666): Revisit in the future to see whether
  // there's a better way to get the right `RenderFrameHost` in cases where the
  // initial empty document is still being used.
  content::RenderFrameHost* frame_host =
      (navigation_handle->HasCommitted() ||
       navigation_handle->IsWaitingToCommit())
          ? navigation_handle->GetRenderFrameHost()
          : content::RenderFrameHost::FromID(
                navigation_handle->GetPreviousRenderFrameHostId());
  // Nothing to do if we've already attached a filter to this document before.
  if (!frame_host || GetFrameFilter(frame_host)) {
    return;
  }

  if (!navigation_handle->HasCommitted() && !is_initial_navigation) {
    // TODO(https://crbug.com/40280666): Figure out how we can have a
    // navigation that is not the initial with no frame filter.
    return;
  }

  bool did_inherit_opener_activation;
  AsyncDocumentSubresourceFilter* filter = FilterForFinishedNavigation(
      navigation_handle, throttle, frame_host, did_inherit_opener_activation);

  if (IsInSubresourceFilterRoot(navigation_handle)) {
    current_committed_load_has_notified_disallowed_load_ = false;
    statistics_.reset();
    if (filter) {
      statistics_ = std::make_unique<subresource_filter::PageLoadStatistics>(
          filter->activation_state(),
          kFingerprintingProtectionRulesetConfig.uma_tag);
      if (filter->activation_state().enable_logging) {
        CHECK(filter->activation_state().activation_level !=
              subresource_filter::mojom::ActivationLevel::kDisabled);
        frame_host->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kWarning,
            kActivationConsoleMessage);
      }
    }
    RecordUmaHistogramsForRootNavigation(
        navigation_handle,
        filter ? filter->activation_state().activation_level
               : subresource_filter::mojom::ActivationLevel::kDisabled,
        did_inherit_opener_activation);
  }
}

void ThrottleManager::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                    const GURL& validated_url) {
  if (!statistics_ || render_frame_host != &page_->GetMainDocument()) {
    return;
  }
  statistics_->OnDidFinishLoad();
}

void ThrottleManager::DidBecomePrimaryPage() {
  CHECK(page_);
  CHECK(page_->IsPrimary());
  // If we tried to notify while non-primary, we didn't notify User Bypass so do
  // that now that the page became primary. This also leads to reattempting
  // notification if a page transitioned from primary to non-primary and back
  // (BFCache).
  if (current_committed_load_has_notified_disallowed_load_) {
    web_contents_helper_->NotifyOnBlockedResources();
  }
}

void ThrottleManager::OnPageCreated(content::Page& page) {
  CHECK(!page.GetMainDocument().IsFencedFrameRoot());
  CHECK(!page_);
  page_ = &page;
}

// Sets the desired page-level `activation_state` for the currently ongoing
// page load, identified by its main-frame `navigation_handle`. If this method
// is not called for a main-frame navigation, the default behavior is no
// activation for that page load.
void ThrottleManager::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const subresource_filter::mojom::ActivationState& activation_state,
    const subresource_filter::ActivationDecision& activation_decision) {
  CHECK(IsInSubresourceFilterRoot(navigation_handle));
  CHECK(!navigation_handle->HasCommitted());

  page_level_activation_computed_ = true;
  page_activation_decision_ = activation_decision;
  page_activation_state_ = activation_state;

  ChildActivationThrottleHandle* throttle_handle =
      ChildActivationThrottleHandle::GetForNavigationHandle(*navigation_handle);
  if (!throttle_handle) {
    return;
  }

  if (activation_state.activation_level ==
      subresource_filter::mojom::ActivationLevel::kDisabled) {
    // If the activation level is disabled, we do not want to run any portion
    // of the filter on this navigation/frame. By deleting the activation
    // throttle handle, we prevent an associated DocumentSubresourceFilter
    // from being created at commit time.
    ChildActivationThrottleHandle::DeleteForNavigationHandle(
        *navigation_handle);
    return;
  }

  if (ruleset_handle_) {
    throttle_handle->throttle()->NotifyPageActivationWithRuleset(
        ruleset_handle_.get(), activation_state);
  }
}

AsyncDocumentSubresourceFilter* ThrottleManager::GetParentFrameFilter(
    content::NavigationHandle* child_frame_navigation) {
  CHECK(!IsInSubresourceFilterRoot(child_frame_navigation));
  return GetFrameFilter(
      child_frame_navigation->GetParentFrameOrOuterDocument());
}

AsyncDocumentSubresourceFilter* ThrottleManager::GetFrameFilter(
    content::RenderFrameHost* frame_host) {
  CHECK(frame_host);
  FilterHandle* filter_handle = FilterHandle::GetForCurrentDocument(frame_host);

  return filter_handle ? filter_handle->filter() : nullptr;
}

const std::optional<subresource_filter::mojom::ActivationState>
ThrottleManager::GetFrameActivationState(content::RenderFrameHost* frame_host) {
  if (AsyncDocumentSubresourceFilter* filter = GetFrameFilter(frame_host)) {
    return filter->activation_state();
  }
  return std::nullopt;
}

void ThrottleManager::LogActivationDecisionUkm(
    content::NavigationHandle* navigation_handle) {
  ukm::SourceId source_id = navigation_handle->GetNextPageUkmSourceId();
  ukm::builders::FingerprintingProtection builder(source_id);

  FilterHandle* filter_handle =
      FilterHandle::GetForCurrentDocument(&page_->GetMainDocument());
  if (!filter_handle) {
    // Without any active filtering, no need to emit ukm.
    return;
  }
  if (filter_handle->filter()->activation_state().activation_level ==
      subresource_filter::mojom::ActivationLevel::kDryRun) {
    builder.SetDryRun(true);
  }
  builder.SetActivationDecision(
      static_cast<int64_t>(page_activation_decision_));
  builder.Record(ukm::UkmRecorder::Get());
}

void ThrottleManager::MaybeNotifyOnBlockedResource(
    content::RenderFrameHost* frame_host) {
  CHECK(page_);
  CHECK_EQ(&GetSubresourceFilterRootPage(frame_host), page_);

  if (current_committed_load_has_notified_disallowed_load_) {
    return;
  }

  FilterHandle* filter_handle =
      FilterHandle::GetForCurrentDocument(&page_->GetMainDocument());
  if (!filter_handle ||
      filter_handle->filter()->activation_state().activation_level ==
          subresource_filter::mojom::ActivationLevel::kDisabled) {
    return;
  }

  if (!filter_handle ||
      filter_handle->filter()->activation_state().activation_level ==
          subresource_filter::mojom::ActivationLevel::kDryRun) {
    return;
  }

  current_committed_load_has_notified_disallowed_load_ = true;

  // Non-primary pages shouldn't affect UI. When the page becomes primary we'll
  // check |current_committed_load_has_notified_disallowed_load_| and try
  // again.
  if (page_->IsPrimary()) {
    web_contents_helper_->NotifyOnBlockedResources();
  }
}

void ThrottleManager::NotifyDisallowLoadPolicy(
    content::NavigationHandle* navigation_handle) {
  LogActivationDecisionUkm(navigation_handle);
}

subresource_filter::mojom::ActivationState
ThrottleManager::ActivationStateForNextCommittedLoad(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNetErrorCode() != net::OK) {
    return subresource_filter::mojom::ActivationState();
  }

  ChildActivationThrottleHandle* throttle_handle =
      ChildActivationThrottleHandle::GetForNavigationHandle(*navigation_handle);
  if (!throttle_handle) {
    return subresource_filter::mojom::ActivationState();
  }

  // Main frame throttles with disabled page-level activation will not have
  // associated filters.
  ActivationStateComputingNavigationThrottle* throttle =
      throttle_handle->throttle();
  AsyncDocumentSubresourceFilter* filter = throttle->filter();
  if (!filter) {
    return subresource_filter::mojom::ActivationState();
  }

  // A filter with DISABLED activation indicates a corrupted ruleset.
  if (filter->activation_state().activation_level ==
      subresource_filter::mojom::ActivationLevel::kDisabled) {
    return subresource_filter::mojom::ActivationState();
  }

  throttle->WillSendActivationToRenderer();
  return filter->activation_state();
}

void ThrottleManager::DidDisallowFirstSubresource() {
  MaybeNotifyOnBlockedResource(receivers_.GetCurrentTargetFrame());
}

void ThrottleManager::CheckActivation(CheckActivationCallback callback) {
  std::move(callback).Run(
      subresource_filter::mojom::ActivationState::New(page_activation_state_));
}

void ThrottleManager::SetDocumentLoadStatistics(
    subresource_filter::mojom::DocumentLoadStatisticsPtr statistics) {
  if (statistics_) {
    statistics_->OnDocumentLoadStatistics(*statistics);
  }
}

AsyncDocumentSubresourceFilter* ThrottleManager::FilterForFinishedNavigation(
    content::NavigationHandle* navigation_handle,
    ActivationStateComputingNavigationThrottle* throttle,
    content::RenderFrameHost* frame_host,
    bool& did_inherit_opener_activation) {
  CHECK(navigation_handle);
  CHECK(frame_host);

  std::unique_ptr<AsyncDocumentSubresourceFilter> filter;
  std::optional<subresource_filter::mojom::ActivationState>
      activation_to_inherit;
  did_inherit_opener_activation = false;

  if (navigation_handle->HasCommitted() && throttle) {
    CHECK_EQ(navigation_handle, throttle->navigation_handle());
    filter = throttle->ReleaseFilter();
  }

  // If the frame should inherit its activation and it has an activated
  // opener/parent, construct a filter with the inherited activation state.
  // The filter's activation state will be available immediately so a throttle
  // is not required. Instead, we construct the filter synchronously.
  if (ShouldInheritOpenerActivation(navigation_handle, frame_host)) {
    content::RenderFrameHost* opener_rfh =
        navigation_handle->GetWebContents()->GetOpener();
    if (auto* opener_throttle_manager = FromPage(opener_rfh->GetPage())) {
      activation_to_inherit =
          opener_throttle_manager->GetFrameActivationState(opener_rfh);
      did_inherit_opener_activation = true;
    }
  } else if (ShouldInheritParentActivation(navigation_handle)) {
    // Throttles are only constructed for navigations handled by the network
    // stack and we only release filters for committed navigations. When a
    // navigation redirects from a URL handled by the network stack to
    // about:blank, a filter can already exist here. We replace it to match
    // behavior for other about:blank frames.
    CHECK(!filter || navigation_handle->GetRedirectChain().size() != 1);
    activation_to_inherit =
        GetFrameActivationState(navigation_handle->GetParentFrame());
  }

  if (activation_to_inherit.has_value() &&
      activation_to_inherit->activation_level !=
          subresource_filter::mojom::ActivationLevel::kDisabled) {
    CHECK(ruleset_handle_);
    // This constructs the filter in a way that allows it to be immediately
    // used. See the AsyncDocumentSubresourceFilter constructor for details.
    filter = std::make_unique<AsyncDocumentSubresourceFilter>(
        ruleset_handle_.get(), frame_host->GetLastCommittedOrigin(),
        activation_to_inherit.value());
  }

  if (!filter) {
    return nullptr;
  }

  // Safe to pass unowned `frame_host` pointer since the filter that owns this
  // callback is owned in the filter handle which will be removed when the
  // RenderFrameHost is deleted.
  base::OnceClosure disallowed_callback(
      base::BindOnce(&ThrottleManager::MaybeNotifyOnBlockedResource,
                     weak_ptr_factory_.GetWeakPtr(), frame_host));
  filter->set_first_disallowed_load_callback(std::move(disallowed_callback));

  AsyncDocumentSubresourceFilter* filter_ptr = filter.get();
  FilterHandle::CreateForCurrentDocument(frame_host, std::move(filter));

  return filter_ptr;
}

void ThrottleManager::RecordUmaHistogramsForRootNavigation(
    content::NavigationHandle* navigation_handle,
    const subresource_filter::mojom::ActivationLevel& activation_level,
    bool did_inherit_opener_activation) {
  UMA_HISTOGRAM_ENUMERATION(
      "FingerprintingProtection.PageLoad.RootNavigation.ActivationState",
      activation_level);
  if (did_inherit_opener_activation) {
    UMA_HISTOGRAM_ENUMERATION(
        "FingerprintingProtection.PageLoad.RootNavigation.ActivationState."
        "DidInherit",
        activation_level);
  }
}

}  // namespace fingerprinting_protection_filter
