// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/feature_manager.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/values.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/common/feature_constants.h"
#include "chromecast/renderer/assistant_bindings.h"
#include "chromecast/renderer/cast_demo_bindings.h"
#include "chromecast/renderer/cast_window_manager_bindings.h"
#include "chromecast/renderer/settings_ui_bindings.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_security_policy.h"

namespace chromecast {

FeatureManager::FeatureManager(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      configured_(false),
      can_install_bindings_(false),
      dev_origin_(GURL()),
      secure_origin_set_(false) {
  registry_.AddInterface(base::BindRepeating(
      &FeatureManager::OnFeatureManagerRequest, base::Unretained(this)));
}

FeatureManager::~FeatureManager() {}

void FeatureManager::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void FeatureManager::OnDestruct() {
  delete this;
}

void FeatureManager::DidClearWindowObject() {
  can_install_bindings_ = true;
  if (!configured_)
    return;

  EnableBindings();
}

void FeatureManager::ConfigureFeatures(
    std::vector<chromecast::shell::mojom::FeaturePtr> features) {
  if (configured_)
    return;
  configured_ = true;
  for (auto& feature : features) {
    // If we want to add enabled/disabled status to FeaturePtr, we can overlap
    // previous setting via [] operator
    features_map_[feature->name] = std::move(feature);
  }

  ConfigureFeaturesInternal();

  if (!can_install_bindings_)
    return;
  EnableBindings();
}

void FeatureManager::ConfigureFeaturesInternal() {
  if (FeatureEnabled(feature::kEnableDevMode)) {
    const base::Value::Dict& dev_mode_config =
        (features_map_.find(feature::kEnableDevMode)->second)->config;
    const std::string* dev_mode_origin =
        dev_mode_config.FindString(feature::kDevModeOrigin);
    DCHECK(dev_mode_origin);
    dev_origin_ = GURL(*dev_mode_origin);
    DCHECK(dev_origin_.is_valid());
  }

  if (FeatureEnabled(feature::kDisableBackgroundSuspend)) {
    auto options = render_frame()->GetRenderFrameMediaPlaybackOptions();
    options.is_background_suspend_enabled = false;
    render_frame()->SetRenderFrameMediaPlaybackOptions(options);
  }

  // Call feature-specific functions.
  SetupAdditionalSecureOrigin();

  // Disable timer throttling for background tabs before the frame is painted.
  if (FeatureEnabled(feature::kDisableBackgroundTabTimerThrottle)) {
    blink::WebRuntimeFeatures::EnableTimerThrottlingForBackgroundTabs(false);
  }
  if (FeatureEnabled(feature::kEnableSettingsUiMojo)) {
    v8_bindings_.insert(new shell::SettingsUiBindings(render_frame()));
  }

  // Window manager bindings will install themselves depending on the specific
  // feature flags enabled, so we pass the feature manager through to let it
  // decide.
  v8_bindings_.insert(
      new shell::CastWindowManagerBindings(render_frame(), this));
  if (FeatureEnabled(feature::kEnableDemoStandaloneMode)) {
    v8_bindings_.insert(new shell::CastDemoBindings(render_frame()));
  }

  if (FeatureEnabled(feature::kEnableAssistantMessagePipe)) {
    auto& feature = GetFeature(feature::kEnableAssistantMessagePipe);
    v8_bindings_.insert(
        new shell::AssistantBindings(render_frame(), feature->config));
  }
}

void FeatureManager::EnableBindings() {
  LOG(INFO) << "Enabling bindings: " << *this;
  for (auto* binding : v8_bindings_) {
    binding->TryInstall();
  }
}

void FeatureManager::OnFeatureManagerRequest(
    mojo::PendingReceiver<shell::mojom::FeatureManager> request) {
  bindings_.Add(this, std::move(request));
}

bool FeatureManager::FeatureEnabled(const std::string& feature) const {
  return base::Contains(features_map_, feature);
}

const chromecast::shell::mojom::FeaturePtr& FeatureManager::GetFeature(
    const std::string& feature) const {
  auto itor = features_map_.find(feature);
  CHECK(itor != features_map_.end(), base::NotFatalUntil::M130);
  return itor->second;
}

void FeatureManager::SetupAdditionalSecureOrigin() {
  if (!dev_origin_.is_valid()) {
    return;
  }

  // Secure origin should be only set once, otherwise it will cause CHECK
  // failure when race between origin safelist changing and thread creation
  // happens (b/63583734).
  if (secure_origin_set_) {
    return;
  }

  secure_origin_set_ = true;

  LOG(INFO) << "Treat origin " << dev_origin_ << " as secure origin";

  blink::WebSecurityPolicy::AddSchemeToSecureContextSafelist(
      blink::WebString::FromASCII(dev_origin_.scheme()));

  network::SecureOriginAllowlist::GetInstance().SetAuxiliaryAllowlist(
      dev_origin_.spec(), nullptr);
}

std::ostream& operator<<(std::ostream& os, const FeatureManager& features) {
  for (auto& feature : features.features_map_) {
    os << feature.first << " ";
  }
  return os;
}

}  // namespace chromecast
