// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ping_persisted_data.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_client_errors.h"

namespace updater {

namespace {

class PingPersistedData : public update_client::PersistedData {
 public:
  PingPersistedData();
  PingPersistedData(const PingPersistedData&) = delete;
  PingPersistedData& operator=(const PingPersistedData&) = delete;
  ~PingPersistedData() override;

  // update_client::PersistedData overrides:
  base::Version GetProductVersion(const std::string& id) const override;
  void SetProductVersion(const std::string& id,
                         const base::Version& pv) override;
  base::Version GetMaxPreviousProductVersion(
      const std::string& id) const override;
  void SetMaxPreviousProductVersion(const std::string& id,
                                    const base::Version& max_version) override;
  std::string GetFingerprint(const std::string& id) const override;
  void SetFingerprint(const std::string& id, const std::string& fp) override;
  int GetDateLastActive(const std::string& id) const override;
  int GetDaysSinceLastActive(const std::string& id) const override;
  void SetDateLastActive(const std::string& id, int dla) override;
  int GetDateLastRollCall(const std::string& id) const override;
  int GetDaysSinceLastRollCall(const std::string& id) const override;
  void SetDateLastRollCall(const std::string& id, int dlrc) override;
  std::string GetCohort(const std::string& id) const override;
  void SetCohort(const std::string& id, const std::string& cohort) override;
  std::string GetCohortName(const std::string& id) const override;
  void SetCohortName(const std::string& id,
                     const std::string& cohort_name) override;
  std::string GetCohortHint(const std::string& id) const override;
  void SetCohortHint(const std::string& id,
                     const std::string& cohort_hint) override;
  std::string GetPingFreshness(const std::string& id) const override;
  void SetDateLastData(const std::vector<std::string>& ids,
                       int datenum,
                       base::OnceClosure callback) override;
  int GetInstallDate(const std::string& id) const override;
  void SetInstallDate(const std::string& id, int install_date) override;
  std::string GetInstallId(const std::string& app_id) const override;
  void SetInstallId(const std::string& app_id,
                    const std::string& install_id) override;
  void GetActiveBits(const std::vector<std::string>& ids,
                     base::OnceCallback<void(const std::set<std::string>&)>
                         callback) const override;
  base::Time GetThrottleUpdatesUntil() const override;
  void SetThrottleUpdatesUntil(base::Time time) override;
  void SetLastUpdateCheckError(
      const update_client::CategorizedError& error) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

PingPersistedData::PingPersistedData() = default;

PingPersistedData::~PingPersistedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::Version PingPersistedData::GetProductVersion(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void PingPersistedData::SetProductVersion(const std::string& id,
                                          const base::Version& pv) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

base::Version PingPersistedData::GetMaxPreviousProductVersion(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void PingPersistedData::SetMaxPreviousProductVersion(
    const std::string& id,
    const base::Version& max_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

std::string PingPersistedData::GetFingerprint(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void PingPersistedData::SetFingerprint(const std::string& id,
                                       const std::string& fingerprint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

int PingPersistedData::GetDateLastActive(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

int PingPersistedData::GetDaysSinceLastActive(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void PingPersistedData::SetDateLastActive(const std::string& id, int dla) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

int PingPersistedData::GetDateLastRollCall(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

int PingPersistedData::GetDaysSinceLastRollCall(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void PingPersistedData::SetDateLastRollCall(const std::string& id, int dlrc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

std::string PingPersistedData::GetCohort(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

void PingPersistedData::SetCohort(const std::string& id,
                                  const std::string& cohort) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

std::string PingPersistedData::GetCohortName(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

void PingPersistedData::SetCohortName(const std::string& id,
                                      const std::string& cohort_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

std::string PingPersistedData::GetCohortHint(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

void PingPersistedData::SetCohortHint(const std::string& id,
                                      const std::string& cohort_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

std::string PingPersistedData::GetPingFreshness(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void PingPersistedData::SetDateLastData(const std::vector<std::string>& ids,
                                        int datenum,
                                        base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

int PingPersistedData::GetInstallDate(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

void PingPersistedData::SetInstallDate(const std::string& id,
                                       int install_date) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

std::string PingPersistedData::GetInstallId(const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "";
}

void PingPersistedData::SetInstallId(const std::string& app_id,
                                     const std::string& install_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Do nothing.
}

void PingPersistedData::GetActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

base::Time PingPersistedData::GetThrottleUpdatesUntil() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void PingPersistedData::SetLastUpdateCheckError(
    const update_client::CategorizedError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void PingPersistedData::SetThrottleUpdatesUntil(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

}  // namespace

std::unique_ptr<update_client::PersistedData> CreatePingPersistedData() {
  return std::make_unique<PingPersistedData>();
}

}  // namespace updater
