// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/component_metrics_provider.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "components/component_updater/component_updater_service.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#include <string>

namespace metrics {

namespace {

SystemProfileProto_ComponentId CrxIdToComponentId(const std::string& app_id) {
  static constexpr auto kComponentMap =
      base::MakeFixedFlatMap<base::StringPiece,
                             SystemProfileProto_ComponentId>({
          {"khaoiebndkojlmppeemjhbpbandiljpe",
           SystemProfileProto_ComponentId_FILE_TYPE_POLICIES},
          {"kfoklmclfodeliojeaekpoflbkkhojea",
           SystemProfileProto_ComponentId_ORIGIN_TRIALS},
          {"llkgjffcdpffmhiakmfcdcblohccpfmo",
           SystemProfileProto_ComponentId_ORIGIN_TRIALS},  // Alternate ID
          {"mimojjlkmoijpicakmndhoigimigcmbb",
           SystemProfileProto_ComponentId_PEPPER_FLASH},
          {"ckjlcfmdbdglblbjglepgnoekdnkoklc",
           SystemProfileProto_ComponentId_PEPPER_FLASH_CHROMEOS},
          {"hnimpnehoodheedghdeeijklkeaacbdc",
           SystemProfileProto_ComponentId_PNACL},
          {"npdjjkjlcidkjlamlmmdelcjbcpdjocm",
           SystemProfileProto_ComponentId_RECOVERY},
          {"giekcmmlnklenlaomppkphknjmnnpneh",
           SystemProfileProto_ComponentId_SSL_ERROR_ASSISTANT},
          {"ojjgnpkioondelmggbekfhllhdaimnho",
           SystemProfileProto_ComponentId_STH_SET},
          {"hfnkpimlhhgieaddgfemjhofmfblmnib",
           SystemProfileProto_ComponentId_CRL_SET},
          {"gcmjkmgdlgnkkcocmoeiminaijmmjnii",
           SystemProfileProto_ComponentId_SUBRESOURCE_FILTER},
          {"gkmgaooipdjhmangpemjhigmamcehddo",
           SystemProfileProto_ComponentId_SW_REPORTER},
          {"oimompecagnajdejgnnjijobebaeigek",
           SystemProfileProto_ComponentId_WIDEVINE_CDM},
          {"bjbdkfoakgmkndalgpadobhgbhhoanho",
           SystemProfileProto_ComponentId_EPSON_INKJET_PRINTER_ESCPR},
          {"ojnjgapiepgciobpecnafnoeaegllfld",
           SystemProfileProto_ComponentId_CROS_TERMINA},
          {"gncenodapghbnkfkoognegdnjoeegmkp",
           SystemProfileProto_ComponentId_STAR_CUPS_DRIVER},
          {"gelhpeofhffbaeegmemklllhfdifagmb",
           SystemProfileProto_ComponentId_SPEECH_SYNTHESIS_SV_SE},
          {"lmelglejhemejginpboagddgdfbepgmp",
           SystemProfileProto_ComponentId_OPTIMIZATION_HINTS},
          {"fookoiellkocclipolgaceabajejjcnp",
           SystemProfileProto_ComponentId_DOWNLOADABLE_STRINGS},
          {"cjfkbpdpjpdldhclahpfgnlhpodlpnba",
           SystemProfileProto_ComponentId_VR_ASSETS},
          {"gjpajnddmedjmcklfflllocelehklffm",
           SystemProfileProto_ComponentId_RTANALYTICS_LIGHT},
          {"mjdmdobabdmfcbaakcaadileafkmifen",
           SystemProfileProto_ComponentId_RTANALYTICS_FULL},
          {"fhbeibbmaepakgdkkmjgldjajgpkkhfj",
           SystemProfileProto_ComponentId_CELLULAR},
          {"ojhpjlocmbogdgmfpkhlaaeamibhnphh",
           SystemProfileProto_ComponentId_ZXCVBN_DATA},
          {"aemllinfpjdgcldgaelcgakpjmaekbai",
           SystemProfileProto_ComponentId_WEBVIEW_APPS_PACKAGE_NAMES_ALLOWLIST},
          {"ggkkehgbnfjpeggfpleeakpidbkibbmn",
           SystemProfileProto_ComponentId_CROWD_DENY},
          {"neifaoindggfcjicffkgpmnlppeffabd",
           SystemProfileProto_ComponentId_MEDIA_FOUNDATION_WIDEVINE_CDM},
      });

  const auto* result = kComponentMap.find(app_id);
  if (result == kComponentMap.end())
    return SystemProfileProto_ComponentId_UNKNOWN;
  return result->second;
}

// Extract the first 32 bits of a fingerprint string, excluding the fingerprint
// format specifier - see the fingerprint format specification at
// https://github.com/google/omaha/blob/master/doc/ServerProtocolV3.md
uint32_t Trim(const std::string& fp) {
  const auto len_prefix = fp.find(".");
  if (len_prefix == std::string::npos)
    return 0;
  uint32_t result = 0;
  if (base::HexStringToUInt(fp.substr(len_prefix + 1, 8), &result))
    return result;
  return 0;
}

}  // namespace

ComponentMetricsProvider::ComponentMetricsProvider(
    std::unique_ptr<ComponentMetricsProviderDelegate> components_info_delegate)
    : components_info_delegate_(std::move(components_info_delegate)) {}

ComponentMetricsProvider::~ComponentMetricsProvider() = default;

void ComponentMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  for (const auto& component : components_info_delegate_->GetComponents()) {
    const auto id = CrxIdToComponentId(component.id);
    // Ignore any unknown components - in practice these are the
    // SupervisedUserWhitelists, which we do not want to transmit to UMA or
    // Crash.
    if (id == SystemProfileProto_ComponentId_UNKNOWN)
      continue;
    auto* proto = system_profile->add_chrome_component();
    proto->set_component_id(id);
    proto->set_version(component.version.GetString());
    proto->set_omaha_fingerprint(Trim(component.fingerprint));
  }
}

}  // namespace metrics
