// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAGIC_BOOST_PUBLIC_CPP_MAGIC_BOOST_STATE_H_
#define CHROMEOS_COMPONENTS_MAGIC_BOOST_PUBLIC_CPP_MAGIC_BOOST_STATE_H_

#include "base/component_export.h"
#include "base/observer_list.h"

namespace chromeos {

enum class HMRConsentStatus : int {
  // User has agreed to consent by pressing "Yes/Agree" button to all dialogs
  // from the consent window.
  kApproved = 0,
  // User has disagreed to consent by pressing "No/Disagree" button to any
  // dialog from the consent window.
  kDeclined = 1,
  // No explicit consent to use the feature has been received yet.
  kPending = 2,
  // No request has been sent to users to collect their consent.
  kUnset = 3,
};

// A class that holds MagicBoost related prefs and states.
class COMPONENT_EXPORT(MAGIC_BOOST) MagicBoostState {
 public:
  // A checked observer which receives MagicBoost state changes.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnHMRConsentStatusUpdated(HMRConsentStatus status) = 0;
  };

  static MagicBoostState* Get();

  MagicBoostState();

  MagicBoostState(const MagicBoostState&) = delete;
  MagicBoostState& operator=(const MagicBoostState&) = delete;

  virtual ~MagicBoostState();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Increments HMRWindowDismissCount count and returns an incremented value.
  // Note that this method is not thread safe, i.e., this increment does NOT
  // operate as an atomic operation. Reading HMRWindowDismissCount immediately
  // after the write can read a stale value.
  virtual int32_t AsyncIncrementHMRConsentWindowDismissCount() = 0;

  // Writes consent status and a respective enabled state to the pref. Note that
  // this method returns BEFORE a write is completed. Reading consent status
  // and/or enabled state immediately after the write can read a stale value.
  virtual void AsyncWriteConsentStatus(HMRConsentStatus consent_status) = 0;

  std::optional<HMRConsentStatus> hmr_consent_status() const {
    return hmr_consent_status_;
  }

  int hmr_consent_window_dismiss_count() const {
    return hmr_consent_window_dismiss_count_;
  }

 protected:
  void UpdateHMRConsentStatus(HMRConsentStatus status);
  void UpdateHMRConsentWindowDismissCount(int32_t count);

 private:
  std::optional<HMRConsentStatus> hmr_consent_status_ =
      HMRConsentStatus::kUnset;
  int32_t hmr_consent_window_dismiss_count_ = 0;

  base::ObserverList<Observer> observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAGIC_BOOST_PUBLIC_CPP_MAGIC_BOOST_STATE_H_
