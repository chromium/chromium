// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LANGUAGE_LANGUAGE_PACKS_LANGUAGE_PACK_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_LANGUAGE_LANGUAGE_PACKS_LANGUAGE_PACK_MANAGER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace ash::language_packs {

// All Language Pack IDs are listed here.
constexpr char kHandwritingFeatureId[] = "LP_ID_HANDWRITING";
constexpr char kTtsFeatureId[] = "LP_ID_TTS";

// Status contains information about the status of a Language Pack operation.
struct PackResult {
  // This string contains the error returns by DLC Service.
  std::string operation_error;

  enum StatusCode {
    UNKNOWN = 0,
    WRONG_ID,
    NOT_INSTALLED,
    IN_PROGRESS,
    INSTALLED
  };

  // The code that indicates the current state of the Pack.
  // INSTALLED means that the Pack is ready to be used.
  StatusCode pack_state;

  // The path where the Pack is available for users to use.
  std::string path;
};

// We define an internal type to identify a Language Pack.
// It's a pair of featured_id and locale that is hashable.
struct PackSpecPair {
  std::string feature_id;
  std::string locale;

  PackSpecPair(const std::string& feature_id, const std::string& locale)
      : feature_id(feature_id), locale(locale) {}

  bool operator==(const PackSpecPair& other) const {
    return (feature_id == other.feature_id && locale == other.locale);
  }

  bool operator!=(const PackSpecPair& other) const { return !(*this == other); }

  // Allows PackSpecPair to be used as a key in STL containers, like flat_map.
  bool operator<(const PackSpecPair& other) const {
    if (feature_id == other.feature_id)
      return locale < other.locale;

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

using OnInstallCompleteCallback =
    base::OnceCallback<void(const PackResult& pack_result)>;
using GetPackStateCallback =
    base::OnceCallback<void(const PackResult& pack_result)>;
using OnUninstallCompleteCallback =
    base::OnceCallback<void(const PackResult& pack_result)>;
using OnInstallBasePackCompleteCallback =
    base::OnceCallback<void(const PackResult& pack_result)>;

// This class manages all Language Packs and their dependencies (called Base
// Packs) on the device.
// This is a Singleton and needs to be accessed via Get().
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

  // Disallow copy and assign.
  LanguagePackManager(const LanguagePackManager&) = delete;
  LanguagePackManager& operator=(const LanguagePackManager&) = delete;

  // Returns true if the given Language Pack exists and can be installed on
  // this device.
  // TODO(claudiomagni): Check per board.
  bool IsPackAvailable(const std::string& feature_id,
                       const std::string& locale);

  // Installs the Language Pack.
  // It takes a callback that will be triggered once the operation is done.
  // A state is passed to the callback.
  void InstallPack(const std::string& feature_id,
                   const std::string& locale,
                   OnInstallCompleteCallback callback);

  // Checks the state of a Language Pack.
  // It takes a callback that will be triggered once the operation is done.
  // A state is passed to the callback.
  // If the state marks the Language Pack as ready, then there's no need to
  // call Install(), otherwise the client should call Install() and not call
  // this method a second time.
  void GetPackState(const std::string& feature_id,
                    const std::string& locale,
                    GetPackStateCallback callback);

  // Features should call this method to indicate that they do not intend to
  // use the Pack again, until they will call |InstallPack()|.
  // The Language Pack will be removed from disk, but no guarantee is given on
  // when that will happen.
  // TODO(claudiomagni): Allow callers to force immediate removal. Useful to
  //                     clear space on disk for another language.
  void RemovePack(const std::string& feature_id,
                  const std::string& locale,
                  OnUninstallCompleteCallback callback);

  // Explicitly installs the base pack for |feature_id|.
  void InstallBasePack(const std::string& feature_id,
                       OnInstallBasePackCompleteCallback callback);

  // Adds an observer to the observer list.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

  // Must be called before using the class.
  void Initialize();

  // Testing only: called to free up resources since this object should never
  // be destroyed.
  void ResetForTesting();

  // Returns the global instance..
  static LanguagePackManager* GetInstance();

 private:
  friend base::NoDestructor<LanguagePackManager>;

  // This class should be accessed only via GetInstance();
  LanguagePackManager();
  ~LanguagePackManager() override;

  // DlcserviceClient::Observer overrides.
  void OnDlcStateChanged(const dlcservice::DlcState& dlc_state) override;

  // Notification method called upon change of DLCs state.
  void NotifyPackStateChanged(const dlcservice::DlcState& dlc_state);

  base::ObserverList<Observer> observers_;
};

}  // namespace ash::language_packs

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_LANGUAGE_PACKS_LANGUAGE_PACK_MANAGER_H_
