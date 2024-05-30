// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/component_metrics_provider.h"

#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "components/component_updater/component_updater_service.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

namespace {

// Extracts the first 32 bits of a fingerprint string, excluding the fingerprint
// format specifier - see the fingerprint format specification at
// https://github.com/google/omaha/blob/master/doc/ServerProtocolV3.md
uint32_t Trim(const std::string& fp) {
  const auto len_prefix = fp.find(".");
  if (len_prefix == std::string::npos) {
    return 0;
  }
  uint32_t result = 0;
  if (base::HexStringToUInt(fp.substr(len_prefix + 1, 8), &result)) {
    return result;
  }
  return 0;
}

}  // namespace

ComponentMetricsProvider::ComponentMetricsProvider(
    std::unique_ptr<ComponentMetricsProviderDelegate> components_info_delegate)
    : components_info_delegate_(std::move(components_info_delegate)) {}

ComponentMetricsProvider::~ComponentMetricsProvider() = default;

// static
SystemProfileProto_ComponentId ComponentMetricsProvider::CrxIdToComponentId(
    const std::string& app_id) {
  static constexpr auto kComponentMap = base::MakeFixedFlatMap<
      std::string_view, SystemProfileProto_ComponentId>({
      {"aagaghndoahmfdbmfnajfklaomcanlnh",
       SystemProfileProto_ComponentId_REAL_TIME_URL_CHECKS_ALLOWLIST},
      {"bjbdkfoakgmkndalgpadobhgbhhoanho",
       SystemProfileProto_ComponentId_EPSON_INKJET_PRINTER_ESCPR},
      {"cdoopinbipdmaefofkedmagbfmdcjnaa",
       SystemProfileProto_ComponentId_CROW_DOMAIN_LIST},
      {"cjfkbpdpjpdldhclahpfgnlhpodlpnba",
       SystemProfileProto_ComponentId_VR_ASSETS},
      {"ckjlcfmdbdglblbjglepgnoekdnkoklc",
       SystemProfileProto_ComponentId_PEPPER_FLASH_CHROMEOS},
      {"cocncanleafgejenidihemfflagifjic",
       SystemProfileProto_ComponentId_COMMERCE_HEURISTICS},
      {"copjbmjbojbakpaedmpkhmiplmmehfck",
       SystemProfileProto_ComponentId_INTERVENTION_POLICY_DATABASE},
      {"dgeeihjgkpfplghdiaomabiakidhjnnn",
       SystemProfileProto_ComponentId_GROWTH_CAMPAIGNS},
      {"dhlpobdgcjafebgbbhjdnapejmpkgiie",
       SystemProfileProto_ComponentId_DESKTOP_SHARING_HUB},
      {"eeigpngbgcognadeebkilcpcaedhellh",
       SystemProfileProto_ComponentId_AUTOFILL_STATES},
      {"efniojlnjndmcbiieegkicadnoecjjef",
       SystemProfileProto_ComponentId_PKI_METADATA},
      {"ehgidpndbllacpjalkiimkbadgjfnnmc",
       SystemProfileProto_ComponentId_THIRD_PARTY_MODULE_LIST},
      {"ehpjbaiafkpkmhjocnenjbbhmecnfcjb",
       SystemProfileProto_ComponentId_LACROS_DOGFOOD_STABLE},
      {"fellaebeeieagcalnmmpapfioejgihci",
       SystemProfileProto_ComponentId_APP_PROVISIONING},
      {"fhbeibbmaepakgdkkmjgldjajgpkkhfj",
       SystemProfileProto_ComponentId_CELLULAR},
      {"fookoiellkocclipolgaceabajejjcnp",
       SystemProfileProto_ComponentId_DOWNLOADABLE_STRINGS},
      {"gcmjkmgdlgnkkcocmoeiminaijmmjnii",
       SystemProfileProto_ComponentId_SUBRESOURCE_FILTER},
      {"gelhpeofhffbaeegmemklllhfdifagmb",
       SystemProfileProto_ComponentId_SPEECH_SYNTHESIS_SV_SE},
      {"ggkkehgbnfjpeggfpleeakpidbkibbmn",
       SystemProfileProto_ComponentId_CROWD_DENY},
      {"ghiclnejioiofblmbphpgbhaojnkempa",
       SystemProfileProto_ComponentId_SMART_DIM},
      {"giekcmmlnklenlaomppkphknjmnnpneh",
       SystemProfileProto_ComponentId_SSL_ERROR_ASSISTANT},
      {"gjpajnddmedjmcklfflllocelehklffm",
       SystemProfileProto_ComponentId_RTANALYTICS_LIGHT},
      {"gkmgaooipdjhmangpemjhigmamcehddo",
       SystemProfileProto_ComponentId_SW_REPORTER},
      {"gncenodapghbnkfkoognegdnjoeegmkp",
       SystemProfileProto_ComponentId_STAR_CUPS_DRIVER},
      {"goaoclndmgofblfopkopecdpfhljclbd",
       SystemProfileProto_ComponentId_SODA_FR_FR},
      {"gonpemdgkjcecdgbnaabipppbmgfggbe",
       SystemProfileProto_ComponentId_FIRST_PARTY_SETS},
      {"hfnkpimlhhgieaddgfemjhofmfblmnib",
       SystemProfileProto_ComponentId_CRL_SET},
      {"hkifppleldbgkdlijbdfkdpedggaopda",
       SystemProfileProto_ComponentId_LACROS_DOGFOOD_CANARY},
      {"hnfmbeciphpghlfgpjfbcdifbknombnk",
       SystemProfileProto_ComponentId_LACROS_DOGFOOD_BETA},
      {"hnimpnehoodheedghdeeijklkeaacbdc",
       SystemProfileProto_ComponentId_PNACL},
      {"icnkogojpkfjeajonkmlplionaamopkf", SystemProfileProto_ComponentId_SODA},
      {"ihnlcenocehgdaegdmhbidjhnhdchfmm",
       SystemProfileProto_ComponentId_RECOVERY_IMPROVED},
      {"imefjhfbkmcmebodilednhmaccmincoa",
       SystemProfileProto_ComponentId_CLIENT_SIDE_PHISHING},
      {"jamhcnnkihinmdlkakkaopbjbbcngflc",
       SystemProfileProto_ComponentId_HYPHENATION},
      {"jclgnikdalajmocbnlgieibfmlejnhmg",
       SystemProfileProto_ComponentId_SODA_DE_DE},
      {"jdmajdolkmhiifibdijabfojmfjmfkpb",
       SystemProfileProto_ComponentId_DEMO_MODE_RESOURCES},
      {"jflhchccmppkfebkiaminageehmchikm",
       SystemProfileProto_ComponentId_THIRD_PARTY_COOKIE_DEPRECATION_METADATA},
      {"jflookgnkcckhobaglndicnbbgbonegd",
       SystemProfileProto_ComponentId_SAFETY_TIPS},
      {"jhefnhlmpagbceldaobdpcjhkknfjohi",
       SystemProfileProto_ComponentId_SODA_IT_IT},
      {"jkcckmaejhmbhagbcebpejbihcnighdb",
       SystemProfileProto_ComponentId_SODA_ES_ES},
      {"kdbdaidmledpgkihpopchgmjikgkjclh",
       SystemProfileProto_ComponentId_DESKTOP_SCREENSHOT_EDITOR},
      {"kfoklmclfodeliojeaekpoflbkkhojea",
       SystemProfileProto_ComponentId_ORIGIN_TRIALS},
      {"khaoiebndkojlmppeemjhbpbandiljpe",
       SystemProfileProto_ComponentId_FILE_TYPE_POLICIES},
      {"kiabhabjdbkjdpjbpigfodbdjmbglcoo",
       SystemProfileProto_ComponentId_TRUST_TOKEN_KEY_COMMITMENTS},
      {"laoigpblnllgcgjnjnllmfolckpjlhki",
       SystemProfileProto_ComponentId_MEI_PRELOAD},
      {"ldobopbhiamakmncndpkeelenhdmgfhk",
       SystemProfileProto_ComponentId_LACROS_DOGFOOD_DEV},
      {"lgmfmojpadlamoidaolfpjpjcondabgm",
       SystemProfileProto_ComponentId_DEMO_MODE_APP},
      {"llkgjffcdpffmhiakmfcdcblohccpfmo",
       SystemProfileProto_ComponentId_ORIGIN_TRIALS},  // Alternate ID
      {"lmelglejhemejginpboagddgdfbepgmp",
       SystemProfileProto_ComponentId_OPTIMIZATION_HINTS},
      {"mfhmdacoffpmifoibamicehhklffanao",
       SystemProfileProto_ComponentId_SCREEN_AI},
      {"mimojjlkmoijpicakmndhoigimigcmbb",
       SystemProfileProto_ComponentId_PEPPER_FLASH},
      {"mjdmdobabdmfcbaakcaadileafkmifen",
       SystemProfileProto_ComponentId_RTANALYTICS_FULL},
      {"neifaoindggfcjicffkgpmnlppeffabd",
       SystemProfileProto_ComponentId_MEDIA_FOUNDATION_WIDEVINE_CDM},
      {"npdjjkjlcidkjlamlmmdelcjbcpdjocm",
       SystemProfileProto_ComponentId_RECOVERY},
      {"obedbbhbpmojnkanicioggnmelmoomoc",
       SystemProfileProto_ComponentId_ON_DEVICE_HEAD_SUGGEST},
      {"oegebmmcimckjhkhbggblnkjloogjdfg",
       SystemProfileProto_ComponentId_SODA_EN_US},
      {"oimompecagnajdejgnnjijobebaeigek",
       SystemProfileProto_ComponentId_WIDEVINE_CDM},
      {"ojhpjlocmbogdgmfpkhlaaeamibhnphh",
       SystemProfileProto_ComponentId_ZXCVBN_DATA},
      {"ojjgnpkioondelmggbekfhllhdaimnho",
       SystemProfileProto_ComponentId_STH_SET},
      {"ojnjgapiepgciobpecnafnoeaegllfld",
       SystemProfileProto_ComponentId_CROS_TERMINA},
      {"onhpjgkfgajmkkeniaoflicgokpaebfa",
       SystemProfileProto_ComponentId_SODA_JA_JP},
      {"cffplpkejcbdpfnfabnjikeicbedmifn",
       SystemProfileProto_ComponentId_MASKED_DOMAIN_LIST},
  });

  const auto result = kComponentMap.find(app_id);
  if (result == kComponentMap.end()) {
    return SystemProfileProto_ComponentId_UNKNOWN;
  }
  return result->second;
}

void ComponentMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  for (const auto& component : components_info_delegate_->GetComponents()) {
    const auto id = ComponentMetricsProvider::CrxIdToComponentId(component.id);
    // Ignore any unknown components - in practice these are the
    // SupervisedUserWhitelists, which we do not want to transmit to UMA or
    // Crash.
    if (id == SystemProfileProto_ComponentId_UNKNOWN) {
      continue;
    }
    auto* proto = system_profile->add_chrome_component();
    proto->set_component_id(id);
    proto->set_version(component.version.GetString());
    proto->set_omaha_fingerprint(Trim(component.fingerprint));
    proto->set_cohort_hash(base::PersistentHash(
        component.cohort_id.substr(0, component.cohort_id.find_last_of(":"))));
  }
}

}  // namespace metrics
