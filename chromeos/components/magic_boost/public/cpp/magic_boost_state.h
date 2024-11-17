// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAGIC_BOOST_PUBLIC_CPP_MAGIC_BOOST_STATE_H_
#define CHROMEOS_COMPONENTS_MAGIC_BOOST_PUBLIC_CPP_MAGIC_BOOST_STATE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/types/expected.h"

namespace chromeos {

// HMR consent is two phases. These are flows and state transitions.
//
// Flow A (Mini Card):
// 1. Mini card is shown (kUnset -> kPendingDisclaimer | kDeclined)
// 2. Disclaimer dialog is shown (kPendingDisclaimer -> kApproved | kDeclined)
//
// *: If a user has pressed [No Thanks] in the mini card, kDeclined is set.
//
// Flow B (Settings):
// 1. A user toggles HMR settings in Settings UI
//    (kUnset | kDeclined -> kPendingDisclaimer)
// 2. Disclaimer dialog is shown (kPendingDisclaimer -> kApproved | kDeclined)
enum class HMRConsentStatus : int {
  // User has agreed to consent by pressing the accept button on the disclaimer
  // UI.
  kApproved = 0,
  // User has disagreed to consent by pressing the decline button on the
  // disclaimer UI or the opt-in card.
  kDeclined = 1,
  // This state is being used when the feature is turned on through the Settings
  // app or a mini card and consent status is unset. In this case, we will show
  // the disclaimer UI when users try to access the Mahi feature through the
  // Mahi menu card.
  kPendingDisclaimer = 2,
  // Users hasn't accept nor decline the consent.
  kUnset = 3,
};

// A class that holds MagicBoost related prefs and states.
class COMPONENT_EXPORT(MAGIC_BOOST) MagicBoostState {
 public:
  // A checked observer which receives MagicBoost state changes.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnMagicBoostEnabledUpdated(bool enabled) {}
    virtual void OnHMREnabledUpdated(bool enabled) {}
    virtual void OnHMRConsentStatusUpdated(HMRConsentStatus status) {}

    // `MagicBoostState` is being deleted. All `ScopedObservation`s MUST get
    // reset. `ScopedObservation::Reset` accesses source (i.e., magic boost
    // state pointer). This is intentionally defined as a pure virtual function
    // as all observers care this.
    virtual void OnIsDeleting() = 0;
  };

  enum class Error {
    kUninitialized,
  };

  static MagicBoostState* Get();

  MagicBoostState();

  MagicBoostState(const MagicBoostState&) = delete;
  MagicBoostState& operator=(const MagicBoostState&) = delete;

  virtual ~MagicBoostState();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Check if the feature is available to use. It will be unavailable in lacros
  // and if mahi is not available.
  virtual bool IsMagicBoostAvailable() = 0;

  // Check if HMR requires the notice banner to appear in the settings page.
  // It will be false in lacros and if the HMR consent status is anything other
  // than Declined.
  virtual bool CanShowNoticeBannerForHMR() = 0;

  // Increments HMRWindowDismissCount count and returns an incremented value.
  // Note that this method is not thread safe, i.e., this increment does NOT
  // operate as an atomic operation. Reading HMRWindowDismissCount immediately
  // after the write can read a stale value.
  virtual int32_t AsyncIncrementHMRConsentWindowDismissCount() = 0;

  // Writes consent status and a respective enabled state to the pref. Note that
  // this method returns BEFORE a write is completed. Reading consent status
  // and/or enabled state immediately after the write can read a stale value.
  virtual void AsyncWriteConsentStatus(HMRConsentStatus consent_status) = 0;

  // Writes HMR enabled value to the pref. Note that this method returns BEFORE
  // a write is completed. Reading consent status and/or enabled state
  // immediately after the write can read a stale value.
  virtual void AsyncWriteHMREnabled(bool enabled) = 0;

  // Indicates if the Orca feature should be included in the opt-in flow.
  virtual void ShouldIncludeOrcaInOptIn(
      base::OnceCallback<void(bool)> callback) {}

  // Marks Orca consent status as rejected and disable the feature.
  virtual void DisableOrcaFeature() = 0;

  // Returns true if Quick Answers or Mahi card should be shown (either consent
  // is approved or pending).
  bool ShouldShowHmrCard();

  base::expected<bool, Error> magic_boost_enabled() const {
    return magic_boost_enabled_;
  }

  base::expected<bool, Error> hmr_enabled() const { return hmr_enabled_; }

  base::expected<HMRConsentStatus, Error> hmr_consent_status() const {
    return hmr_consent_status_;
  }

  int hmr_consent_window_dismiss_count() const {
    return hmr_consent_window_dismiss_count_;
  }

 protected:
  void UpdateMagicBoostEnabled(bool enabled);
  void UpdateHMREnabled(bool enabled);
  void UpdateHMRConsentStatus(HMRConsentStatus status);
  void UpdateHMRConsentWindowDismissCount(int32_t count);

 private:
  void NotifyOnIsDeleting();

  // Use `base::expected` instead of `std::optional` to avoid implicit bool
  // conversion: https://abseil.io/tips/141.
  base::expected<bool, Error> magic_boost_enabled_ =
      base::unexpected(Error::kUninitialized);
  base::expected<bool, Error> hmr_enabled_ =
      base::unexpected(Error::kUninitialized);
  base::expected<HMRConsentStatus, Error> hmr_consent_status_ =
      base::unexpected(Error::kUninitialized);
  int32_t hmr_consent_window_dismiss_count_ = 0;

  base::ObserverList<Observer> observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAGIC_BOOST_PUBLIC_CPP_MAGIC_BOOST_STATE_H_
