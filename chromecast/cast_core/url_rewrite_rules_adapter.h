// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_URL_REWRITE_RULES_ADAPTER_H_
#define CHROMECAST_CAST_CORE_URL_REWRITE_RULES_ADAPTER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/common/identification_settings_manager.h"
#include "chromecast/common/mojom/identification_settings.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/cast_core/public/src/proto/v2/url_rewrite.pb.h"

namespace chromecast {

// This represents a SubstitutableParameter in the context of the
// IdentificationSettingsManager API.  These must all be set because of the
// conventions used in the gRPC API.
struct ParamRule {
  ParamRule();
  ~ParamRule();

  ParamRule(const ParamRule&) = delete;
  ParamRule(ParamRule&&);
  ParamRule& operator=(const ParamRule&) = delete;
  ParamRule& operator=(ParamRule&&);

  std::string name;
  std::string replace_token;
  std::string suppress_token;
  std::string value;
};

// This collectively represents all of our input to the
// IdentificationSettingsManager API: DeviceSettings, AppSettings, and
// SubstitutableParameters.
struct TranslatedRewriteRules {
  TranslatedRewriteRules();
  ~TranslatedRewriteRules();

  TranslatedRewriteRules(const TranslatedRewriteRules&) = delete;
  TranslatedRewriteRules(TranslatedRewriteRules&&);
  TranslatedRewriteRules& operator=(const TranslatedRewriteRules&) = delete;
  TranslatedRewriteRules& operator=(TranslatedRewriteRules&&);

  // DeviceSettings from IdentificationSettingsManager.
  base::flat_map<std::string, std::string> static_headers;
  base::flat_map<std::string, std::string> url_replacements;

  // AppSettings from IdentificationSettingsManager.
  std::vector<std::string> full_host_names;
  std::vector<std::string> wildcard_host_names;

  // SubstitutableParameters from IdentificationSettingsManager.
  std::vector<ParamRule> params;
};

// This represents all-in-one rewrite rules translated into Mojo format in
// order to be passed to IdentificationSettingsManager.
struct MojoIdentificationSettings {
  MojoIdentificationSettings(const cast::v2::UrlRequestRewriteRules& rules);
  MojoIdentificationSettings() = delete;
  ~MojoIdentificationSettings();

  std::vector<mojom::SubstitutableParameterPtr> substitutable_params;
  mojom::AppSettingsPtr application_settings;
  mojom::DeviceSettingsPtr device_settings;
};

// This class is responsible for taking URL rewrite rules as specified by the
// gRPC proto and translating them to the IdentificationSettingsManager API.
// Because the gRPC API is much more general, there is a lot of potential for
// specifying something that's not possible in the latter API.  Therefore, this
// class has to make strong assumptions about the form the input rules can take
// to be valid.  These assumptions are based on the current similar situation of
// converting from IdentificationSettingsManager to the FIDL URL rewrite rule
// API.
class UrlRewriteRulesAdapter final : public mojom::ClientAuthDelegate {
 public:
  explicit UrlRewriteRulesAdapter(
      const cast::v2::UrlRequestRewriteRules& rules);
  ~UrlRewriteRulesAdapter() override;

  void UpdateRules(const cast::v2::UrlRequestRewriteRules& rules);

  // |remote_settings_manager| is supported by a RenderFrameHost's
  // IPC::ChannelProxy to the render process that hosts its corresponding
  // RenderFrame.
  void AddRenderFrame(
      mojo::AssociatedRemote<mojom::IdentificationSettingsManager>
          remote_settings_manager);

 private:
  struct FrameInfo {
    FrameInfo();
    ~FrameInfo();

    FrameInfo(const FrameInfo&) = delete;
    FrameInfo(FrameInfo&&);
    FrameInfo& operator=(const FrameInfo&) = delete;
    FrameInfo& operator=(FrameInfo&&);

    bool operator<(const FrameInfo& other) const;

    mojo::AssociatedRemote<mojom::IdentificationSettingsManager>
        settings_manager;
    mojo::ReceiverId auth_delegate_id;
  };

  void OnRenderFrameRemoved(
      mojom::IdentificationSettingsManagerProxy* settings_manager_proxy);

  // mojom::ClientAuthDelegate overrides.
  void EnsureCerts(EnsureCertsCallback callback) override;
  void EnsureSignature(EnsureSignatureCallback callback) override;

  // Current set of rewrite rules.
  TranslatedRewriteRules rules_;

  // IdentificationSettingsManager mojo remote and ClientAuthDelegate binding ID
  // for each known frame.
  base::flat_set<FrameInfo> remote_settings_managers_;

  mojo::ReceiverSet<mojom::ClientAuthDelegate> auth_delegate_bindings_;

  base::WeakPtrFactory<UrlRewriteRulesAdapter> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_URL_REWRITE_RULES_ADAPTER_H_
