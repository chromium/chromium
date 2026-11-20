// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/ntp_renderer_keep_alive.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/extension_url_overrides.h"
#endif

namespace {

constexpr char kConsumerName[] = "NtpRendererKeepAlive";

constexpr base::MemoryConsumerTraits kMemoryConsumerTraits(
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone,
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    base::MemoryConsumerTraits::SupportsMemoryLimit::kNo,
    base::MemoryConsumerTraits::InProcess::kNo,
    base::MemoryConsumerTraits::RecreateMemoryCost::kExpensive);

bool IsWebUiNtpActiveForProfile(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return false;
  }
  if (const PrefService* prefs = profile->GetPrefs()) {
    const std::string ntp_location =
        prefs->GetString(prefs::kNewTabPageLocationOverride);
    if (!ntp_location.empty()) {
      return GURL(ntp_location) == chrome::ChromeUINewTabPageURLAsGURL();
    }
  }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  GURL url = chrome::ChromeUINewTabURLAsGURL();
  if (ExtensionUrlOverrides::HandleChromeURLOverride(&url, profile)) {
    return false;
  }
#endif
  return search::GetNewTabPageURL(profile) ==
         chrome::ChromeUINewTabPageURLAsGURL();
}

}  // namespace

// static
void NtpRendererKeepAlive::MaybePin(Profile* profile,
                                    content::RenderProcessHost* rph) {
  if (!base::FeatureList::IsEnabled(ntp_features::kKeepNtpRendererAlive) ||
      !profile || profile->ShutdownStarted()) {
    return;
  }
  static constexpr char kNtpRendererKeepAliveKey[] = "ntp_renderer_keep_alive";
  auto* keep_alive = static_cast<NtpRendererKeepAlive*>(
      profile->GetUserData(kNtpRendererKeepAliveKey));
  // Ongoing RAM and multi-profile bounds rely on `MemoryConsumer` releasing on
  // moderate+ memory pressure; `IsProcessLimitReached()` is a commit-time
  // guard for explicit `--renderer-process-limit` or OS `RLIMIT_NPROC` caps.
  if (!IsWebUiNtpActiveForProfile(profile) ||
      content::RenderProcessHost::IsProcessLimitReached()) {
    if (keep_alive) {
      keep_alive->Release(/*decrement=*/true);
    }
    return;
  }
  if (!keep_alive) {
    auto created = std::make_unique<NtpRendererKeepAlive>(profile);
    keep_alive = created.get();
    profile->SetUserData(kNtpRendererKeepAliveKey, std::move(created));
  }
  keep_alive->SetHost(rph);
}

NtpRendererKeepAlive::NtpRendererKeepAlive(Profile* profile)
    : profile_(profile),
      memory_consumer_registration_(
          kConsumerName,
          kMemoryConsumerTraits,
          this,
          base::MemoryConsumerRegistration::CheckUnregister::kDisabled) {
  CHECK(profile_);
  profile_observation_.Observe(profile_);
}

NtpRendererKeepAlive::~NtpRendererKeepAlive() {
  Release(/*decrement=*/true);
}

void NtpRendererKeepAlive::SetHost(content::RenderProcessHost* rph) {
  if (!rph ||
      memory_limit() <= base::MemoryLimit::ModeratePressureThreshold() ||
      !rph->IsInitializedAndNotDead() || rph->IsDeletingSoon() ||
      rph->AreRefCountsDisabled()) {
    return;
  }
  if (host_ == rph) {
    return;
  }
  Release(/*decrement=*/true);
  host_ = rph;
  host_observation_.Observe(rph);
  if (TemplateURLService* service =
          TemplateURLServiceFactory::GetForProfile(profile_)) {
    template_url_service_observation_.Observe(service);
  }
  if (PrefService* prefs = profile_->GetPrefs()) {
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.AddMultiple(
        {
            prefs::kNewTabPageLocationOverride,
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
            ExtensionUrlOverrides::kExtensionURLOverrides,
#endif
        },
        base::BindRepeating(&NtpRendererKeepAlive::OnActiveNtpMightHaveChanged,
                            base::Unretained(this)));
  }
  rph->IncrementPendingReuseRefCount();
}

void NtpRendererKeepAlive::Release(bool decrement) {
  content::RenderProcessHost* host = std::exchange(host_, nullptr);
  host_observation_.Reset();
  template_url_service_observation_.Reset();
  pref_change_registrar_.Reset();
  if (host && decrement && !host->IsDeletingSoon() &&
      !host->AreRefCountsDisabled()) {
    host->DecrementPendingReuseRefCount();
  }
}

void NtpRendererKeepAlive::OnActiveNtpMightHaveChanged() {
  if (host_ && profile_ && !IsWebUiNtpActiveForProfile(profile_)) {
    Release(/*decrement=*/true);
  }
}

void NtpRendererKeepAlive::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  Release(/*decrement=*/true);
}

void NtpRendererKeepAlive::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  Release(/*decrement=*/false);
}

void NtpRendererKeepAlive::OnUpdateMemoryLimit() {}

void NtpRendererKeepAlive::OnReleaseMemory() {
  if (memory_limit() <= base::MemoryLimit::ModeratePressureThreshold()) {
    Release(/*decrement=*/true);
  }
}

void NtpRendererKeepAlive::OnProfileWillBeDestroyed(Profile* profile) {
  Release(/*decrement=*/true);
  profile_observation_.Reset();
  profile_ = nullptr;
}

void NtpRendererKeepAlive::OnTemplateURLServiceChanged() {
  OnActiveNtpMightHaveChanged();
}

void NtpRendererKeepAlive::OnTemplateURLServiceShuttingDown() {
  Release(/*decrement=*/true);
}
