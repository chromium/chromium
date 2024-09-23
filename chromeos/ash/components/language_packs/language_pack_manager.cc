// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/language_packs/language_pack_manager.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/language_packs/handwriting.h"
#include "chromeos/ash/components/language_packs/language_packs_util.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

using ::ash::input_method::InputMethodManager;

namespace ash::language_packs {
namespace {

LanguagePackManager* g_instance = nullptr;

const base::flat_map<std::string, std::string>& GetAllBasePackDlcIds() {
  // Map of all features and corresponding Base Pack DLC IDs.
  static const base::NoDestructor<base::flat_map<std::string, std::string>>
      all_dlc_ids({
          {kHandwritingFeatureId, "handwriting-base"},
      });

  return *all_dlc_ids;
}

// Finds the ID of the DLC corresponding to the Base Pack for a feature.
// Returns the DLC ID if the feature has a Base Pack or std::nullopt
// otherwise.
std::optional<std::string> GetDlcIdForBasePack(const std::string& feature_id) {
  // We search in the static list for the given |feature_id|.
  const auto it = GetAllBasePackDlcIds().find(feature_id);

  if (it == GetAllBasePackDlcIds().end()) {
    return std::nullopt;
  }

  return it->second;
}

// Run a callback later in the current `SingleThreadedTaskRunner`.
void RunCallbackLater(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}

void InstallDlc(const std::string& dlc_id,
                DlcserviceClient::InstallCallback callback) {
  DlcserviceClient* client = DlcserviceClient::Get();
  if (client) {
    dlcservice::InstallRequest install_request;
    install_request.set_id(dlc_id);
    client->Install(install_request, std::move(callback), base::DoNothing());
  } else {
    CHECK_IS_TEST();
    DlcserviceClient::InstallResult result;
    result.error = dlcservice::kErrorInternal;

    RunCallbackLater(base::BindOnce(std::move(callback), std::move(result)));
  }
}

void GetDlcState(const std::string& dlc_id,
                 DlcserviceClient::GetDlcStateCallback callback) {
  DlcserviceClient* client = DlcserviceClient::Get();
  if (client) {
    client->GetDlcState(dlc_id, std::move(callback));
  } else {
    CHECK_IS_TEST();
    dlcservice::DlcState state;
    state.set_id(dlc_id);
    state.set_state(dlcservice::DlcState::State::DlcState_State_NOT_INSTALLED);
    RunCallbackLater(base::BindOnce(
        std::move(callback), dlcservice::kErrorInternal, std::move(state)));
  }
}

void UninstallDlc(const std::string& dlc_id,
                  DlcserviceClient::UninstallCallback callback) {
  DlcserviceClient* client = DlcserviceClient::Get();
  if (client) {
    client->Uninstall(dlc_id, std::move(callback));
  } else {
    CHECK_IS_TEST();
    RunCallbackLater(
        base::BindOnce(std::move(callback), dlcservice::kErrorInternal));
  }
}

// Warning: These DLCs are guaranteed to be downloaded (is_trusted), but not
// guaranteed to be installed (state == INSTALLED).
void GetExistingDlcs(DlcserviceClient::GetExistingDlcsCallback callback) {
  DlcserviceClient* client = DlcserviceClient::Get();
  if (client) {
    client->GetExistingDlcs(std::move(callback));
  } else {
    CHECK_IS_TEST();
    RunCallbackLater(base::BindOnce(std::move(callback),
                                    dlcservice::kErrorInternal,
                                    dlcservice::DlcsWithContent()));
  }
}

void OnInstallDlcComplete(OnInstallCompleteCallback callback,
                          const std::string& feature_id,
                          const std::string& locale,
                          const DlcserviceClient::InstallResult& dlc_result) {
  PackResult result = ConvertDlcInstallResultToPackResult(dlc_result);
  result.feature_id = feature_id;
  result.language_code = locale;

  const bool success = result.operation_error == PackResult::ErrorCode::kNone;
  if (!success) {
    if (feature_id == kHandwritingFeatureId) {
      base::UmaHistogramEnumeration(
          "ChromeOS.LanguagePacks.InstallError.Handwriting",
          GetDlcErrorTypeForUma(dlc_result.error));
    } else if (feature_id == kTtsFeatureId) {
      base::UmaHistogramEnumeration("ChromeOS.LanguagePacks.InstallError.Tts",
                                    GetDlcErrorTypeForUma(dlc_result.error));
    } else if (feature_id == kFontsFeatureId) {
      base::UmaHistogramEnumeration("ChromeOS.LanguagePacks.InstallError.Fonts",
                                    GetDlcErrorTypeForUma(dlc_result.error));
    }
  }

  base::UmaHistogramEnumeration("ChromeOS.LanguagePacks.InstallPack.Success",
                                GetSuccessValueForUma(feature_id, success));

  std::move(callback).Run(result);
}

void OnUninstallDlcComplete(OnUninstallCompleteCallback callback,
                            std::string_view feature_id,
                            const std::string& locale,
                            std::string_view err) {
  PackResult result;
  result.feature_id = feature_id;
  result.language_code = locale;
  result.operation_error = ConvertDlcErrorToErrorCode(err);

  const bool success = err == dlcservice::kErrorNone;
  if (success) {
    result.pack_state = PackResult::StatusCode::kNotInstalled;
  } else {
    result.pack_state = PackResult::StatusCode::kUnknown;
  }

  base::UmaHistogramBoolean("ChromeOS.LanguagePacks.UninstallComplete.Success",
                            success);

  std::move(callback).Run(result);
}

void OnGetDlcState(GetPackStateCallback callback,
                   std::string feature_id,
                   const std::string& locale,
                   std::string_view err,
                   const dlcservice::DlcState& dlc_state) {
  PackResult result;
  if (dlc_state.is_verified() &&
      dlc_state.state() == dlcservice::DlcState_State_NOT_INSTALLED) {
    // Mount the DLC for the client if it already exists on disk.
    // By pure coincidence, `GetPackStateCallback` is the same as
    // `OnInstallCompleteCallback`, so we can directly pass in the
    // client-supplied callback here.
    InstallDlc(dlc_state.id(),
               base::BindOnce(&OnInstallDlcComplete, std::move(callback),
                              std::move(feature_id), std::move(locale)));
    return;
  }

  // GetDlcState() returns 2 errors:
  // one for the DBus call and one for the actual DLC.
  // If the first error is set we can ignore the DLC state.
  if (err.empty() || err == dlcservice::kErrorNone) {
    result = ConvertDlcStateToPackResult(dlc_state);
  } else {
    result.operation_error = ConvertDlcErrorToErrorCode(err);
    result.pack_state = PackResult::StatusCode::kUnknown;
  }

  result.feature_id = feature_id;
  result.language_code = locale;

  std::move(callback).Run(result);
}

// This functions goes through the list of locales to install and remove,
// according to the diff. It performs the actual installation and uninstallation
// of DLCs on the device.
// It should be called whenever Input Methods are changed.
void InstallOrRemoveToMatchState(const std::string& feature_id,
                                 const StringsDiff& locale_diff) {
  for (const std::string& locale : locale_diff.remove) {
    LanguagePackManager::RemovePack(feature_id, locale, base::DoNothing());
  }
  for (const std::string& locale : locale_diff.add) {
    LanguagePackManager::InstallPack(feature_id, locale, base::DoNothing());
  }
}

// Updates packs for input methods based on the user prefs and the currently
// installed DLCs.
// TODO: b/294162606 - Write unit tests for this function if possible.
void UpdateFromInputMethodPrefs(
    base::span<const std::string> installed_hwr_locales,
    input_method::InputMethodUtil* input_method_util,
    PrefService* prefs) {
  const std::vector<std::string> input_method_ids =
      ExtractInputMethodsFromPrefs(prefs);
  const base::flat_set<std::string> target_hwr_locales = MapThenFilterStrings(
      input_method_ids, base::BindRepeating(MapInputMethodIdToHandwritingLocale,
                                            input_method_util));

  const StringsDiff locale_diff = ComputeStringsDiff(
      {installed_hwr_locales.begin(), installed_hwr_locales.end()},
      target_hwr_locales);

  InstallOrRemoveToMatchState(kHandwritingFeatureId, locale_diff);
}

// Callback for dlcservice::GetExistingDlcs().
// TODO: b/294162606 - Write unit tests for this function if possible.
void OnGetExistingDlcs(PrefService* prefs,
                       std::string_view err,
                       const dlcservice::DlcsWithContent& dlcs_with_content) {
  if (!err.empty() && err != dlcservice::kErrorNone) {
    DLOG(ERROR) << "DlcserviceClient::GetExisingDlcs() returned error";
    // TODO: b/285985206 - Record a UMA histogram.
    return;
  }

  const base::flat_set<std::string> hwr_locales =
      ConvertDlcsWithContentToHandwritingLocales(dlcs_with_content);
  UpdateFromInputMethodPrefs({hwr_locales.begin(), hwr_locales.end()},
                             InputMethodManager::Get()->GetInputMethodUtil(),
                             prefs);
}

}  // namespace

const base::flat_map<PackSpecPair, std::string>& GetAllLanguagePackDlcIds() {
  // Map of all DLCs and corresponding IDs.
  // It's a map from PackSpecPair to DLC ID. The pair is <feature id, locale>.
  // Whenever a new DLC is created, it needs to be added here.
  // Clients of Language Packs don't need to know the IDs.
  static const base::NoDestructor<base::flat_map<PackSpecPair, std::string>>
      all_dlc_ids({
          // Handwriting Recognition.
          {{kHandwritingFeatureId, "am"}, "handwriting-am"},
          {{kHandwritingFeatureId, "ar"}, "handwriting-ar"},
          {{kHandwritingFeatureId, "be"}, "handwriting-be"},
          {{kHandwritingFeatureId, "bg"}, "handwriting-bg"},
          {{kHandwritingFeatureId, "bn"}, "handwriting-bn"},
          {{kHandwritingFeatureId, "ca"}, "handwriting-ca"},
          {{kHandwritingFeatureId, "cs"}, "handwriting-cs"},
          {{kHandwritingFeatureId, "da"}, "handwriting-da"},
          {{kHandwritingFeatureId, "de"}, "handwriting-de"},
          {{kHandwritingFeatureId, "el"}, "handwriting-el"},
          {{kHandwritingFeatureId, "en"}, "handwriting-en"},
          {{kHandwritingFeatureId, "es"}, "handwriting-es"},
          {{kHandwritingFeatureId, "et"}, "handwriting-et"},
          {{kHandwritingFeatureId, "fa"}, "handwriting-fa"},
          {{kHandwritingFeatureId, "fi"}, "handwriting-fi"},
          {{kHandwritingFeatureId, "fil"}, "handwriting-fil"},
          {{kHandwritingFeatureId, "fr"}, "handwriting-fr"},
          {{kHandwritingFeatureId, "ga"}, "handwriting-ga"},
          {{kHandwritingFeatureId, "gu"}, "handwriting-gu"},
          {{kHandwritingFeatureId, "hi"}, "handwriting-hi"},
          {{kHandwritingFeatureId, "hr"}, "handwriting-hr"},
          {{kHandwritingFeatureId, "hu"}, "handwriting-hu"},
          {{kHandwritingFeatureId, "hy"}, "handwriting-hy"},
          {{kHandwritingFeatureId, "id"}, "handwriting-id"},
          {{kHandwritingFeatureId, "is"}, "handwriting-is"},
          {{kHandwritingFeatureId, "it"}, "handwriting-it"},
          {{kHandwritingFeatureId, "iw"}, "handwriting-iw"},
          {{kHandwritingFeatureId, "ja"}, "handwriting-ja"},
          {{kHandwritingFeatureId, "ka"}, "handwriting-ka"},
          {{kHandwritingFeatureId, "kk"}, "handwriting-kk"},
          {{kHandwritingFeatureId, "km"}, "handwriting-km"},
          {{kHandwritingFeatureId, "kn"}, "handwriting-kn"},
          {{kHandwritingFeatureId, "ko"}, "handwriting-ko"},
          {{kHandwritingFeatureId, "lo"}, "handwriting-lo"},
          {{kHandwritingFeatureId, "lt"}, "handwriting-lt"},
          {{kHandwritingFeatureId, "lv"}, "handwriting-lv"},
          {{kHandwritingFeatureId, "ml"}, "handwriting-ml"},
          {{kHandwritingFeatureId, "mn"}, "handwriting-mn"},
          {{kHandwritingFeatureId, "mr"}, "handwriting-mr"},
          {{kHandwritingFeatureId, "ms"}, "handwriting-ms"},
          {{kHandwritingFeatureId, "mt"}, "handwriting-mt"},
          {{kHandwritingFeatureId, "my"}, "handwriting-my"},
          {{kHandwritingFeatureId, "ne"}, "handwriting-ne"},
          {{kHandwritingFeatureId, "nl"}, "handwriting-nl"},
          {{kHandwritingFeatureId, "no"}, "handwriting-no"},
          {{kHandwritingFeatureId, "or"}, "handwriting-or"},
          {{kHandwritingFeatureId, "pa"}, "handwriting-pa"},
          {{kHandwritingFeatureId, "pl"}, "handwriting-pl"},
          {{kHandwritingFeatureId, "pt"}, "handwriting-pt"},
          {{kHandwritingFeatureId, "ro"}, "handwriting-ro"},
          {{kHandwritingFeatureId, "ru"}, "handwriting-ru"},
          {{kHandwritingFeatureId, "si"}, "handwriting-si"},
          {{kHandwritingFeatureId, "sk"}, "handwriting-sk"},
          {{kHandwritingFeatureId, "sl"}, "handwriting-sl"},
          {{kHandwritingFeatureId, "sr"}, "handwriting-sr"},
          {{kHandwritingFeatureId, "sv"}, "handwriting-sv"},
          {{kHandwritingFeatureId, "ta"}, "handwriting-ta"},
          {{kHandwritingFeatureId, "te"}, "handwriting-te"},
          {{kHandwritingFeatureId, "th"}, "handwriting-th"},
          {{kHandwritingFeatureId, "ti"}, "handwriting-ti"},
          {{kHandwritingFeatureId, "tr"}, "handwriting-tr"},
          {{kHandwritingFeatureId, "uk"}, "handwriting-uk"},
          {{kHandwritingFeatureId, "ur"}, "handwriting-ur"},
          {{kHandwritingFeatureId, "vi"}, "handwriting-vi"},
          {{kHandwritingFeatureId, "zh"}, "handwriting-zh"},
          {{kHandwritingFeatureId, "zh-HK"}, "handwriting-zh-HK"},

          // Text-To-Speech.
          {{kTtsFeatureId, "bn"}, "tts-bn-bd-c"},
          {{kTtsFeatureId, "cs"}, "tts-cs-cz-c"},
          {{kTtsFeatureId, "da"}, "tts-da-dk-c"},
          {{kTtsFeatureId, "de"}, "tts-de-de-c"},
          {{kTtsFeatureId, "el"}, "tts-el-gr-c"},
          {{kTtsFeatureId, "en-au"}, "tts-en-au-c"},
          {{kTtsFeatureId, "en-gb"}, "tts-en-gb-c"},
          {{kTtsFeatureId, "en-us"}, "tts-en-us-c"},
          {{kTtsFeatureId, "es-es"}, "tts-es-es-c"},
          {{kTtsFeatureId, "es-us"}, "tts-es-us-c"},
          {{kTtsFeatureId, "fi"}, "tts-fi-fi-c"},
          {{kTtsFeatureId, "fil"}, "tts-fil-ph-c"},
          {{kTtsFeatureId, "fr"}, "tts-fr-fr-c"},
          {{kTtsFeatureId, "hi"}, "tts-hi-in-c"},
          {{kTtsFeatureId, "hu"}, "tts-hu-hu-c"},
          {{kTtsFeatureId, "id"}, "tts-id-id-c"},
          {{kTtsFeatureId, "it"}, "tts-it-it-c"},
          {{kTtsFeatureId, "ja"}, "tts-ja-jp-c"},
          {{kTtsFeatureId, "km"}, "tts-km-kh-c"},
          {{kTtsFeatureId, "ko"}, "tts-ko-kr-c"},
          {{kTtsFeatureId, "nb"}, "tts-nb-no-c"},
          {{kTtsFeatureId, "ne"}, "tts-ne-np-c"},
          {{kTtsFeatureId, "nl"}, "tts-nl-nl-c"},
          {{kTtsFeatureId, "pl"}, "tts-pl-pl-c"},
          {{kTtsFeatureId, "pt-br"}, "tts-pt-br-c"},
          {{kTtsFeatureId, "pt-pt"}, "tts-pt-pt-c"},
          {{kTtsFeatureId, "si"}, "tts-si-lk-c"},
          {{kTtsFeatureId, "sk"}, "tts-sk-sk-c"},
          {{kTtsFeatureId, "sv"}, "tts-sv-se-c"},
          {{kTtsFeatureId, "th"}, "tts-th-th-c"},
          {{kTtsFeatureId, "tr"}, "tts-tr-tr-c"},
          {{kTtsFeatureId, "uk"}, "tts-uk-ua-c"},
          {{kTtsFeatureId, "vi"}, "tts-vi-vn-c"},
          {{kTtsFeatureId, "yue"}, "tts-yue-hk-c"},

          // Fonts.
          {{kFontsFeatureId, "ja"}, "extrafonts-ja"},
          {{kFontsFeatureId, "ko"}, "extrafonts-ko"},
      });

  return *all_dlc_ids;
}

// TODO: b/294162606 - Calling this function with a `std::string_view` or a
// `const char*` argument causes two string copies per argument - one to call
// the function, and one to create the `PackSpecPair` to look up in the map.
// Either refactor this function to take in a `std::string_view` to reduce it
// down to one string copy per argument, use heterogeneous lookup
// (https://abseil.io/tips/144) to reduce it down to zero string copies, or
// rewrite this function completely.
std::optional<std::string> GetDlcIdForLanguagePack(
    const std::string& feature_id,
    const std::string& locale) {
  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(feature_id, locale);
  const auto it = GetAllLanguagePackDlcIds().find(spec);

  if (it == GetAllLanguagePackDlcIds().end()) {
    return std::nullopt;
  }

  return it->second;
}

std::optional<std::string> DlcToTtsLocale(std::string_view dlc_id) {
  const base::flat_map<PackSpecPair, std::string>& all_ids =
      GetAllLanguagePackDlcIds();
  // Relies on the fact that TTS `PackSpecPair`s are "grouped together" in the
  // sorted `flat_map`.
  auto it = all_ids.upper_bound({kTtsFeatureId, ""});
  while (it != all_ids.end() && it->first.feature_id == kTtsFeatureId) {
    if (it->second == dlc_id) {
      return it->first.locale;
    }
    ++it;
  }

  return std::nullopt;
}

///////////////////////////////////////////////////////////
// PackResult constructors and destructors.
PackResult::PackResult() {
  this->pack_state = PackResult::StatusCode::kUnknown;
}

PackResult::~PackResult() = default;

PackResult::PackResult(const PackResult&) = default;
///////////////////////////////////////////////////////////

bool LanguagePackManager::IsPackAvailable(const std::string& feature_id,
                                          const std::string& input_locale) {
  const std::string locale = ResolveLocale(feature_id, input_locale);

  // We search in the static list for the given Pack spec.
  const PackSpecPair spec(feature_id, locale);
  return base::Contains(GetAllLanguagePackDlcIds(), spec);
}

void LanguagePackManager::InstallPack(const std::string& feature_id,
                                      const std::string& input_locale,
                                      OnInstallCompleteCallback callback) {
  const std::string locale = ResolveLocale(feature_id, input_locale);
  const std::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  InstallDlc(*dlc_id, base::BindOnce(&OnInstallDlcComplete, std::move(callback),
                                     feature_id, locale));
}

void LanguagePackManager::GetPackState(const std::string& feature_id,
                                       const std::string& input_locale,
                                       GetPackStateCallback callback) {
  const std::string locale = ResolveLocale(feature_id, input_locale);
  const std::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  // TODO: b/351723265 - Split this language code metric into a metric for each
  // feature.
  base::UmaHistogramSparse("ChromeOS.LanguagePacks.GetPackState.LanguageCode",
                           static_cast<int32_t>(base::PersistentHash(locale)));
  base::UmaHistogramEnumeration("ChromeOS.LanguagePacks.GetPackState.FeatureId",
                                GetFeatureIdValueForUma(feature_id));

  GetDlcState(*dlc_id, base::BindOnce(&OnGetDlcState, std::move(callback),
                                      feature_id, locale));
}

void LanguagePackManager::RemovePack(const std::string& feature_id,
                                     const std::string& input_locale,
                                     OnUninstallCompleteCallback callback) {
  const std::string locale = ResolveLocale(feature_id, input_locale);
  const std::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(feature_id, locale);

  // If the given Language Pack doesn't exist, run callback and don't reach the
  // DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  UninstallDlc(*dlc_id,
               base::BindOnce(&OnUninstallDlcComplete, std::move(callback),
                              feature_id, locale));
}

void LanguagePackManager::InstallBasePack(
    const std::string& feature_id,
    OnInstallBasePackCompleteCallback callback) {
  const std::optional<std::string> dlc_id = GetDlcIdForBasePack(feature_id);

  // If the given |feature_id| doesn't have a Base Pack, run callback and
  // don't reach the DLC Service.
  if (!dlc_id) {
    std::move(callback).Run(CreateInvalidDlcPackResult());
    return;
  }

  base::UmaHistogramEnumeration(
      "ChromeOS.LanguagePacks.InstallBasePack.FeatureId",
      GetFeatureIdValueForUma(feature_id));

  InstallDlc(*dlc_id, base::BindOnce(&OnInstallDlcComplete, std::move(callback),
                                     feature_id, ""));
}

void LanguagePackManager::UpdatePacksForOobe(
    const std::string& input_locale,
    OnUpdatePacksForOobeCallback callback) {
  if (!IsOobe()) {
    DLOG(ERROR) << "Language Packs: UpdatePackForOobe called while not in OOBE";
    return;
  }

  // For now, TTS is the only feature we want to install during OOBE.
  // In the future we'll have a function that returns the list of features to
  // install.
  const std::string locale = ResolveLocale(kTtsFeatureId, input_locale);
  const std::optional<std::string> dlc_id =
      GetDlcIdForLanguagePack(kTtsFeatureId, locale);

  if (dlc_id) {
    base::UmaHistogramBoolean("ChromeOS.LanguagePacks.Oobe.ValidLocale", true);
    InstallDlc(*dlc_id,
               base::BindOnce(&OnInstallDlcComplete, std::move(callback),
                              kTtsFeatureId, locale));
  } else {
    base::UmaHistogramBoolean("ChromeOS.LanguagePacks.Oobe.ValidLocale", false);
    DLOG(ERROR) << "Language Packs: UpdatePacksForOobe locale does not exist";
    std::move(callback).Run(CreateInvalidDlcPackResult());
  }
}

void LanguagePackManager::CheckAndUpdateDlcsForInputMethods(
    PrefService* pref_service) {
  // The list of input methods have changed. We need to get the list of current
  // DLCs installed on device, which is an asynchronous method.
  GetExistingDlcs(base::BindOnce(&OnGetExistingDlcs, pref_service));
}

void LanguagePackManager::ObservePrefs(PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This is the main gate for the functionality of observing Prefs.
  // If this flag is false, all of the cascading logic is disabled.
  if (base::FeatureList::IsEnabled(features::kLanguagePacksInSettings)) {
    pref_change_registrar_.Init(pref_service);
    base::RepeatingClosure callback = base::BindRepeating(
        &LanguagePackManager::CheckAndUpdateDlcsForInputMethods, pref_service);
    pref_change_registrar_.Add(ash::prefs::kLanguagePreloadEngines, callback);
  }
}

void LanguagePackManager::AddObserver(Observer* const observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void LanguagePackManager::RemoveObserver(Observer* const observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void LanguagePackManager::NotifyPackStateChanged(
    std::string_view feature_id,
    std::string_view locale,
    const dlcservice::DlcState& dlc_state) {
  PackResult result = ConvertDlcStateToPackResult(dlc_state);
  result.feature_id = feature_id;
  result.language_code = locale;
  for (Observer& observer : observers_) {
    observer.OnPackStateChanged(result);
  }
}

void LanguagePackManager::OnDlcStateChanged(
    const dlcservice::DlcState& dlc_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::optional<std::string> handwriting_locale =
      DlcToHandwritingLocale(dlc_state.id());
  if (handwriting_locale.has_value()) {
    NotifyPackStateChanged(kHandwritingFeatureId, *handwriting_locale,
                           dlc_state);
  }

  const std::optional<std::string> tts_locale = DlcToTtsLocale(dlc_state.id());
  if (tts_locale.has_value()) {
    NotifyPackStateChanged(kTtsFeatureId, *tts_locale, dlc_state);
  }
}

LanguagePackManager::LanguagePackManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!g_instance);
  g_instance = this;
  DlcserviceClient* client = DlcserviceClient::Get();
  if (client) {
    obs_.Observe(client);
  } else {
    CHECK_IS_TEST();
    // No observation.
  }
}

LanguagePackManager::~LanguagePackManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(g_instance, this);
  pref_change_registrar_.RemoveAll();
  g_instance = nullptr;
}

void LanguagePackManager::Initialise() {
  // Heap-allocates an instance, which is then set in `g_instance` in the
  // constructor.
  // This instance will be cleaned up in `Shutdown()`.
  // Calling this while `g_instance` is set will result in a `CHECK` failure
  // instead of a memory leak.
  new LanguagePackManager();
}

void LanguagePackManager::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
LanguagePackManager* LanguagePackManager::GetInstance() {
  return g_instance;
}

}  // namespace ash::language_packs
