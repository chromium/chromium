// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_LANGUAGE_PACK_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_LANGUAGE_PACK_MANAGER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/ash/input_method_util.h"

class PrefService;

namespace ash::language_packs {

// All Language Pack IDs are listed here.
inline constexpr char kHandwritingFeatureId[] = "LP_ID_HANDWRITING";
inline constexpr char kTtsFeatureId[] = "LP_ID_TTS";
inline constexpr char kFontsFeatureId[] = "LP_ID_FONT";

// Feature IDs.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// See enum LanguagePackFeatureIds in tools/metrics/histograms/enums.xml.
enum class FeatureIdsEnum {
  kUnknown = 0,
  kHandwriting = 1,
  kTts = 2,
  kFonts = 3,
  kMaxValue = kFonts,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// See enum LanguagePackFeatureSuccess in tools/metrics/histograms/enums.xml.
enum class FeatureSuccessEnum {
  kUnknownSuccess = 0,
  kUnknownFailure = 1,
  kHandwritingSuccess = 2,
  kHandwritingFailure = 3,
  kTtsSuccess = 4,
  kTtsFailure = 5,
  kFontsSuccess = 6,
  kFontsFailure = 7,
  kMaxValue = kFontsFailure,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// See enum LanguagePackDlcErrorType in tools/metrics/histograms/enums.xml.
enum class DlcErrorTypeEnum {
  kErrorUnknown = 0,
  kErrorNone = 1,
  kErrorInternal = 2,
  kErrorBusy = 3,
  kErrorNeedReboot = 4,
  kErrorInvalidDlc = 5,
  kErrorAllocation = 6,
  kErrorNoImageFound = 7,
  kMaxValue = kErrorNoImageFound,
};

// Status contains information about the status of a Language Pack operation.
struct PackResult {
  // Needed for Complex type checker.
  PackResult();
  ~PackResult();
  PackResult(const PackResult&);

  enum class StatusCode {
    kUnknown = 0,
    kNotInstalled,
    kInProgress,
    kInstalled
  };

  enum class ErrorCode {
    kNone = 0,
    kOther,
    kWrongId,
    kNeedReboot,
    kAllocation
  };

  // The code that indicates the current state of the Pack.
  // kInstalled means that the Pack is ready to be used.
  // If there's any error during the operation, we set status to kUnknown.
  StatusCode pack_state;

  // If there is any error in the operation that is requested, it is indicated
  // here.
  ErrorCode operation_error;

  // The feature ID of the pack.
  std::string feature_id;

  // The resolved language code that this Pack is associated with.
  // Often this field matches the locale requested by the client, but due to
  // various mappings between languages, regions and variants, it might be
  // different.
  // This is set only if the input locale is valid; undetermined otherwise.
  std::string language_code;

  // The path where the Pack is available for users to use.
  std::string path;
};

// We define an internal type to identify a Language Pack.
// It's a pair of featured_id and locale that is hashable.
struct PackSpecPair {
  std::string feature_id;
  std::string locale;

  PackSpecPair(std::string feature_id, std::string locale)
      : feature_id(std::move(feature_id)), locale(std::move(locale)) {}

  bool operator==(const PackSpecPair& other) const {
    return (feature_id == other.feature_id && locale == other.locale);
  }

  bool operator!=(const PackSpecPair& other) const { return !(*this == other); }

  // Allows PackSpecPair to be used as a key in STL containers, like flat_map.
  bool operator<(const PackSpecPair& other) const {
    if (feature_id == other.feature_id) {
      return locale < other.locale;
    }

    return feature_id < other.feature_id;
  }

  // Simple hash function: XOR the string hash.
  struct HashFunction {
    size_t operator()(const PackSpecPair& obj) const {
      size_t first_hash = std::hash<std::string>()(obj.feature_id);
      size_t second_hash = std::hash<std::string>()(obj.locale) << 1;
      return first_hash ^ second_hash;
    }
  };
};

// Returns a static mapping from `PackSpecPair`s to DLC IDs.
// Internal only, do not use - this function will likely be removed in the
// future.
const base::flat_map<PackSpecPair, std::string>& GetAllLanguagePackDlcIds();

// Finds the ID of the DLC corresponding to the given spec.
// Returns the DLC ID if the DLC exists or std::nullopt otherwise.
std::optional<std::string> GetDlcIdForLanguagePack(
    const std::string& feature_id,
    const std::string& locale);

using OnInstallCompleteCallback =
    base::OnceCallback<void(const PackResult& pack_result)>;
using GetPackStateCallback =
    base::OnceCallback<void(const PackResult& pack_result)>;
using OnUninstallCompleteCallback =
    base::OnceCallback<void(const PackResult& pack_result)>;
using OnInstallBasePackCompleteCallback =
    base::OnceCallback<void(const PackResult& pack_result)>;
using OnUpdatePacksForOobeCallback =
    base::OnceCallback<void(const PackResult& pack_result)>;

// This class manages all Language Packs and their dependencies (called Base
// Packs) on the device.
// This is a Singleton and needs to be accessed via Get().
//
// Sequencing: This class is sequence-checked so all accesses to it - non-static
// methods, `Initialise()` and `Shutdown()` - should be done on the same
// sequence. This may be overly strict, see b/319906094 for more details.
class LanguagePackManager : public DlcserviceClient::Observer {
 public:
  // Observer of Language Packs.
  // TODO(crbug.com/1194688): Make the Observers dependent on feature and
  // locale, so that clients don't get notified for things they are not
  // interested in.
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the state of a Language Pack changes, which includes
    // installation, download, removal or errors.
    virtual void OnPackStateChanged(const PackResult& pack_result) = 0;
  };

  // Do not use unless in tests.
  // Only one `LanguagePackManager` can be instantiated at any time.
  // Use `GetInstance()` instead to obtain the currently instantiated instance,
  // likely instantiated by `Initialise()`.
  LanguagePackManager();

  // Disallow copy and assign.
  LanguagePackManager(const LanguagePackManager&) = delete;
  LanguagePackManager& operator=(const LanguagePackManager&) = delete;

  ~LanguagePackManager() override;

  // Returns true if the given Language Pack exists and can be installed on
  // this device.
  // TODO(claudiomagni): Check per board.
  static bool IsPackAvailable(const std::string& feature_id,
                              const std::string& locale);

  // Installs the Language Pack.
  // It takes a callback that will be triggered once the operation is done.
  // A state is passed to the callback.
  static void InstallPack(const std::string& feature_id,
                          const std::string& locale,
                          OnInstallCompleteCallback callback);

  // Checks the state of a Language Pack.
  // It takes a callback that will be triggered once the operation is done.
  // A state is passed to the callback.
  // If the state marks the Language Pack as ready, then there's no need to
  // call Install(), otherwise the client should call Install() and not call
  // this method a second time.
  // This will automatically mount the DLC if it exists on disk (is_verified),
  // and return a PackState of kInstalled.
  static void GetPackState(const std::string& feature_id,
                           const std::string& locale,
                           GetPackStateCallback callback);

  // Features should call this method to indicate that they do not intend to
  // use the Pack again, until they will call |InstallPack()|.
  // The Language Pack will be removed from disk, but no guarantee is given on
  // when that will happen.
  // TODO(claudiomagni): Allow callers to force immediate removal. Useful to
  //                     clear space on disk for another language.
  static void RemovePack(const std::string& feature_id,
                         const std::string& locale,
                         OnUninstallCompleteCallback callback);

  // Explicitly installs the base pack for |feature_id|.
  static void InstallBasePack(const std::string& feature_id,
                              OnInstallBasePackCompleteCallback callback);

  // Installs relevant language packs during OOBE.
  // This method should only be called during OOBE and will do nothing if called
  // outside it.
  static void UpdatePacksForOobe(const std::string& locale,
                                 OnUpdatePacksForOobeCallback callback);

  // Registers itself as an Observer of all the relevant languages Prefs.
  void ObservePrefs(PrefService* pref_service);

  // Adds an observer to the observer list.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

  // Initialises the global instance. This is typically called from
  // ash_dbus_helper.h's `InitializeDBus()`, which is called from
  // `ChromeMainDelegate::PostEarlyInitialization()`.
  // Cannot be called multiple times - `GetInstance()` must return `nullptr`
  // before this static method is called.
  // Requires the global `DlcserviceClient` to be initialised.
  // Do not use this in tests, instantiate a test-local `LanguagePackManager`
  // instead.
  static void Initialise();

  // Shuts down the global instance. This is typically called from
  // ash_dbus_helper.h's `ShutdownDBus()`, which is called from
  // `ChromeBrowserMainPartsAsh::PostDestroyThreads()`.
  // Cannot be called multiple times - `GetInstance()` must return a non-null
  // pointer before this static method is called.
  // The global `DlcserviceClient` at the time of initialisation must still
  // exist when this is called.
  // Do not use this in tests, the destructor of the test-local
  // `LanguagePackManager` will correctly unset the currently instantiated
  // instance.
  static void Shutdown();

  // Returns the currently instantiated instance. This is typically the global
  // instance, but may be a test-local `LanguagePackManager` during tests.
  static LanguagePackManager* GetInstance();

 private:
  // Retrieves the list of installed DLCs and updates Packs accordingly.
  // This function should be called when LPM initializes and then each time
  // Prefs change.
  static void CheckAndUpdateDlcsForInputMethods(PrefService* pref_service);

  // DlcserviceClient::Observer overrides.
  void OnDlcStateChanged(const dlcservice::DlcState& dlc_state) override;

  // Notification method called upon change of DLCs state.
  void NotifyPackStateChanged(std::string_view feature_id,
                              std::string_view locale,
                              const dlcservice::DlcState& dlc_state)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<Observer> observers_;
  base::ScopedObservation<DlcserviceClient, DlcserviceClient::Observer> obs_{
      this};
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace ash::language_packs

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_LANGUAGE_PACK_MANAGER_H_
